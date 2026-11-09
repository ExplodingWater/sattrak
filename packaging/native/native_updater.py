#!/usr/bin/env python3
"""Per-user sattrak installation and transactional updates (macOS / Debian).

Only a managed installation can be updated. System packages and source trees
are never overwritten. No shell commands, elevated privileges or pip modules.
"""
import argparse
import contextlib
import ctypes
import fcntl
import functools
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import stat
import subprocess
import struct
import sys
import tarfile
import tempfile
import time

MAX_ARCHIVE = 2 * 1024**3
MAX_UNPACKED = 4 * 1024**3
PAYLOAD = {'ag', 'native_updater.py', 'release-version.txt', 'platform.txt',
           'assets', 'earth_texture.jpg', 'LICENSE', 'TERMS.md'}
VERSION = re.compile(r'v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
                     r'(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?'
                     r'(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?\Z')


def version(value):
    match = VERSION.fullmatch(value)
    if not match or len(value) > 128:
        raise ValueError('Invalid release version')
    core = tuple(int(n) for n in match.group(1, 2, 3))
    if any(n > 2**32 - 1 for n in core):
        raise ValueError('Invalid release version')
    preview = match.group(4)
    parts = preview.split('.') if preview else []
    if any(p.isdecimal() and len(p) > 1 and p[0] == '0' for p in parts):
        raise ValueError('Invalid preview version')
    # Stable sorts above previews; numeric identifiers below strings.
    return core, not bool(parts), tuple((0, int(p)) if p.isdecimal() else (1, p) for p in parts)


def binary_platform(path):
    """Inspect the executable, independent of CMake/Python running under Rosetta."""
    with Path(path).open('rb') as binary:
        header = binary.read(64)
    if len(header) < 20:
        raise ValueError('Invalid native executable')
    if header[:4] in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf'):
        endian = '<' if header[0] == 0xcf else '>'
        cpu = struct.unpack_from(endian + 'I', header, 4)[0]
        if cpu == 0x0100000c:
            return 'macos-arm64'
        if cpu == 0x01000007:
            return 'macos-x64'
    if header[:4] == b'\x7fELF' and header[4] == 2 and header[5] in (1, 2):
        cpu = struct.unpack_from(('<' if header[5] == 1 else '>') + 'H', header, 18)[0]
        if cpu == 183:
            return 'debian-arm64'
        if cpu == 62:
            return 'debian-amd64'
    raise ValueError('Only single-architecture 64-bit Mach-O / ELF packages are supported')


@functools.lru_cache(maxsize=1)
def platform_id():
    machine = platform.machine().lower()
    if machine not in ('arm64', 'aarch64', 'x86_64', 'amd64'):
        raise ValueError('Unsupported CPU architecture')
    arm = machine in ('arm64', 'aarch64')
    if sys.platform == 'darwin':
        # An Intel Python can run through Rosetta on Apple Silicon. Detect the
        # hardware in that case rather than labeling an ARM payload as Intel.
        if not arm:
            value, size = ctypes.c_int(), ctypes.c_size_t(ctypes.sizeof(ctypes.c_int))
            try:
                libc = ctypes.CDLL(None)
                if libc.sysctlbyname(b'hw.optional.arm64', ctypes.byref(value), ctypes.byref(size), None, 0) == 0:
                    arm = bool(value.value)
            except (AttributeError, OSError):
                pass
            if not arm:
                # Some sandboxes deny sysctl. Ask the kernel to execute a
                # system ARM binary; this also works from an Intel Python.
                try:
                    arm = subprocess.run(['/usr/bin/arch', '-arm64', '/usr/bin/true'],
                                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                         timeout=3).returncode == 0
                except (OSError, subprocess.SubprocessError):
                    pass
        return 'macos-arm64' if arm else 'macos-x64'
    if sys.platform == 'linux':
        os_release = Path('/etc/os-release').read_text()
        if not any(word in os_release.lower() for word in ('debian', 'ubuntu')):
            raise ValueError('This native package requires Debian or Ubuntu')
        return 'debian-arm64' if arm else 'debian-amd64'
    raise ValueError('Unsupported operating system')


def owned_directory(path):
    info = path.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o022:
        raise ValueError('Installation must be a directory owned and writable only by this user')


def managed_root(path):
    # Reject a symlink root instead of following it to an unintended installation.
    path = Path(os.path.abspath(path))
    owned_directory(path)
    path = path.resolve(strict=True)
    marker = json.loads((path / 'install.json').read_text())
    if marker != {'schema': 1, 'platform': platform_id()}:
        raise ValueError('Not a matching managed sattrak installation')
    current = (path / 'current').resolve(strict=True)
    if current.parent != path / 'releases' or not current.is_dir():
        raise ValueError('Invalid active release')
    return path


@contextlib.contextmanager
def verified_archive(path, digest, expected_size=None):
    if not re.fullmatch('[0-9a-f]{64}', digest):
        raise ValueError('Invalid SHA-256 digest')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
    with os.fdopen(fd, 'rb') as archive:
        info = os.fstat(archive.fileno())
        if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= MAX_ARCHIVE:
            raise ValueError('Invalid update file')
        if expected_size is not None and info.st_size != expected_size:
            raise ValueError('Incomplete update file')
        hasher = hashlib.sha256()
        for block in iter(lambda: archive.read(1024 * 1024), b''):
            hasher.update(block)
        if hasher.hexdigest() != digest:
            raise ValueError('Update SHA-256 verification failed')
        archive.seek(0)
        yield archive


def unpack(archive, stage):
    """Extract only ordinary files into a fresh private directory, never links."""
    with tarfile.open(fileobj=archive, mode='r:gz') as package:
        entries = []
        seen = set()
        total = 0
        # Iterate with a bound rather than loading arbitrarily many headers.
        for member in package:
            name = PurePosixPath(member.name)
            if (len(entries) >= 100000 or name.is_absolute() or '..' in name.parts or
                    '\\' in member.name or not name.parts or name.parts[0] != 'sattrak' or
                    (len(name.parts) > 1 and name.parts[1] not in PAYLOAD) or
                    (not member.isfile() and not member.isdir()) or
                    member.issym() or member.islnk() or member.size < 0):
                raise ValueError('Unsafe update archive member')
            relative = Path(*name.parts[1:])
            if len(name.parts) == 1:
                if not member.isdir():
                    raise ValueError('Invalid archive root')
                continue
            if relative in seen:
                raise ValueError('Duplicate update archive member')
            seen.add(relative)
            total += member.size
            if total > MAX_UNPACKED:
                raise ValueError('Unpacked update is too large')
            entries.append((member, relative))
        for member, relative in entries:
            destination = stage / relative
            if member.isdir():
                destination.mkdir(parents=True, exist_ok=True)
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            source = package.extractfile(member)
            if source is None:
                raise ValueError('Missing archive file')
            with source, destination.open('xb') as output:
                shutil.copyfileobj(source, output)
            destination.chmod(0o755 if relative == Path('ag') else 0o644)
    for name in PAYLOAD - {'assets'}:
        if not (stage / name).is_file():
            raise ValueError('Incomplete native release: ' + name)
    if not (stage / 'assets').is_dir():
        raise ValueError('Native release has no assets')
    release = (stage / 'release-version.txt').read_text().strip()
    version(release)
    if (stage / 'platform.txt').read_text().strip() != platform_id():
        raise ValueError('Update is for another operating system or architecture')
    return release


@contextlib.contextmanager
def installation_lock(root, exclusive=True, timeout=0):
    fd = os.open(root / 'session.lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    try:
        deadline = time.monotonic() + timeout
        while True:
            try:
                fcntl.flock(fd, (fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH) | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Update postponed: another sattrak session is open')
                time.sleep(0.05)
        yield fd
    finally:
        os.close(fd)


def switch_current(root, release_folder):
    """Atomic pointer switch; a failed extraction never touches the live release."""
    temporary = root / ('.current-' + str(os.getpid()))
    try:
        temporary.symlink_to(Path('releases') / release_folder.name, target_is_directory=True)
        os.replace(temporary, root / 'current')
    finally:
        temporary.unlink(missing_ok=True)


def apply_update(archive_path, root, digest, size, tag):
    root = managed_root(root)
    archive_path = Path(archive_path)
    cache = root / 'user-data/updates'
    if archive_path.parent.resolve() != cache.resolve():
        raise ValueError('Update file must be in this installation cache')
    version(tag)
    with installation_lock(root, timeout=2):
        current = (root / 'current/release-version.txt').read_text().strip()
        if version(tag) <= version(current):
            raise ValueError('Update skipped: installed version is current')
        with tempfile.TemporaryDirectory(prefix='.stage-', dir=root / 'releases') as temporary:
            stage = Path(temporary)
            with verified_archive(archive_path, digest, size) as archive:
                release = unpack(archive, stage)
            if version(release) != version(tag):
                raise ValueError('Package version does not match the selected release')
            # A digest distinguishes rebuilt payloads without treating metadata as
            # an upgrade. Keep the previous release for recovery / manual rollback.
            destination = root / 'releases' / (release.lstrip('v') + '-' + digest[:12])
            if destination.exists():
                raise ValueError('Release directory already exists; inspect the previous attempt')
            stage.rename(destination)
            try:
                switch_current(root, destination)
            except BaseException:
                shutil.rmtree(destination)
                raise
        archive_path.unlink(missing_ok=True)
    return 'Update installed: ' + release


def install(archive_path, root, digest, size=None):
    root = Path(os.path.abspath(root))
    # Initial installation must not absorb or replace an existing source tree.
    if root.exists() and any(root.iterdir()):
        raise ValueError('Choose an empty installation directory')
    root.mkdir(parents=True, exist_ok=True, mode=0o700)
    owned_directory(root)
    root = root.resolve(strict=True)
    (root / 'releases').mkdir(mode=0o700)
    data = root / 'user-data'
    (data / 'updates').mkdir(parents=True, mode=0o700)
    try:
        with installation_lock(root):
            with tempfile.TemporaryDirectory(prefix='.stage-', dir=root / 'releases') as temporary:
                stage = Path(temporary)
                with verified_archive(archive_path, digest, size) as archive:
                    release = unpack(archive, stage)
                destination = root / 'releases' / (release.lstrip('v') + '-' + digest[:12])
                stage.rename(destination)
                switch_current(root, destination)
            for name in ('assets', 'earth_texture.jpg', 'TERMS.md', 'LICENSE'):
                (data / name).symlink_to(Path('../current') / name)
            (root / 'install.json').write_text(json.dumps({'schema': 1, 'platform': platform_id()}))
            launcher = root / 'sattrak'
            launcher.write_text('#!/bin/sh\nexec python3 "$(dirname "$0")/current/native_updater.py" --launch "$(dirname "$0")" "$@"\n')
            launcher.chmod(0o755)
    except BaseException:
        # This root was empty and created exclusively for installation.
        # Remove only contents created here; the source archive is elsewhere.
        for name in ('releases', 'user-data'):
            shutil.rmtree(root / name, ignore_errors=True)
        for name in ('current', 'install.json', 'sattrak', 'session.lock'):
            (root / name).unlink(missing_ok=True)
        raise
    return 'Installed ' + release + '; launch ' + str(root / 'sattrak')


def launch(root, arguments):
    root = managed_root(root)
    with installation_lock(root, exclusive=False, timeout=30):
        # Resolve the active release while locked. The app also holds a shared
        # lock, so terminating this launcher cannot permit an in-use update.
        program = (root / 'current/ag').resolve(strict=True)
        return subprocess.run([str(program), *arguments], cwd=root / 'user-data', close_fds=True).returncode


def wait_for_parent(pid):
    if pid <= 1 or pid == os.getpid():
        raise ValueError('Invalid parent process')
    deadline = time.monotonic() + 180
    while True:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return
        if time.monotonic() >= deadline:
            raise RuntimeError('Update postponed: parent is still running')
        time.sleep(0.1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--verify', type=Path)
    action.add_argument('--install', type=Path)
    action.add_argument('--apply', type=Path)
    action.add_argument('--launch', type=Path)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--sha256')
    parser.add_argument('--size', type=int)
    parser.add_argument('--tag')
    parser.add_argument('--parent', type=int)
    args, rest = parser.parse_known_args()
    result = None
    try:
        if args.launch:
            return launch(args.launch, rest)
        if rest:
            raise ValueError('Unexpected arguments')
        if args.verify:
            with verified_archive(args.verify, args.sha256 or '', args.size):
                pass
            return 0
        if os.geteuid() == 0:
            raise ValueError('Install and update as your normal user, without sudo')
        if not args.root or not args.sha256:
            raise ValueError('--root and --sha256 are required')
        if args.install:
            print(install(args.install, args.root, args.sha256, args.size))
        else:
            root = managed_root(args.root)
            result = root / 'user-data/updates/result.txt'
            # Drop the terminal session; installation proceeds after app exit.
            try:
                os.setsid()
            except OSError:
                pass
            if not args.parent or not args.tag or args.size is None:
                raise ValueError('--parent, --tag and --size are required')
            wait_for_parent(args.parent)
            message = apply_update(args.apply, root, args.sha256, args.size, args.tag)
            result.write_text(message + '\n')
            print(message)
        return 0
    except Exception as error:
        message = str(error)
        if result:
            result.write_text(message + '\n')
        print(message, file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
