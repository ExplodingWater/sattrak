#!/usr/bin/env python3
"""Build the versioned, architecture-specific payload used by native updates."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import shutil
import tarfile
import tempfile


def load_updater(source):
    spec = importlib.util.spec_from_file_location('native_updater', source / 'packaging/native/native_updater.py')
    updater = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(updater)
    return updater


def stage_payload(binary, source, version, destination):
    updater = load_updater(source)
    updater.version(version)
    if version.startswith('v'):
        raise ValueError('Use a release version without a leading v')
    native_platform = updater.binary_platform(binary)
    destination.mkdir(parents=True)
    shutil.copy2(binary, destination / 'ag')
    shutil.copy2(source / 'packaging/native/native_updater.py', destination / 'native_updater.py')
    for name in ('earth_texture.jpg', 'LICENSE', 'TERMS.md'):
        shutil.copy2(source / name, destination / name)
    shutil.copytree(source / 'assets', destination / 'assets')
    notices = destination / 'assets/third-party-licenses'
    notices.mkdir(exist_ok=True)
    for library, name in (('glfw', 'LICENSE.md'), ('imgui', 'LICENSE.txt'), ('implot', 'LICENSE'),
                          ('miniaudio', 'LICENSE'), ('sgp4', 'LICENSE'), ('nlohmann', 'LICENSE.MIT')):
        shutil.copy2(source / 'external' / library / name, notices / (library + '-' + name))
    (destination / 'release-version.txt').write_text(version + '\n')
    (destination / 'platform.txt').write_text(native_platform + '\n')
    return native_platform


def checksum(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    path.with_name(path.name + '.sha256').write_text(digest.hexdigest() + '  ' + path.name + '\n')
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--platform', choices=('macos-arm64', 'macos-x64', 'debian-amd64', 'debian-arm64'))
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    helper = args.source / 'packaging/native/native_updater.py'
    updater = load_updater(args.source)
    updater.version(args.version)
    if args.version.startswith('v'):
        parser.error('Use a version without a leading v; release tags may include v')
    detected = updater.binary_platform(args.binary)
    if args.platform and args.platform != detected:
        parser.error('Package platform does not match the actual executable')
    args.platform = detected
    args.output.mkdir(parents=True, exist_ok=True)
    destination = args.output / f'sattrak-{args.version}-{args.platform}.tar.gz'
    with tempfile.TemporaryDirectory(prefix='sattrak-package-') as temporary:
        stage = Path(temporary) / 'sattrak'
        stage_payload(args.binary, args.source, args.version, stage)
        # Avoid links / special files, private paths and host ownership metadata.
        def metadata(info):
            if not (info.isfile() or info.isdir()):
                raise ValueError('Payload contains a link or special file: ' + info.name)
            info.uid = info.gid = 0
            info.uname = info.gname = ''
            info.mode = 0o755 if info.isdir() or info.name == 'sattrak/ag' else 0o644
            return info
        with tarfile.open(destination, 'w:gz', compresslevel=6) as archive:
            archive.add(stage, arcname='sattrak', filter=metadata)
    checksum(destination)
    # Bootstrap helper is also a release artifact for the first installation.
    shutil.copy2(helper, args.output / 'native_updater.py')
    print(destination)


if __name__ == '__main__':
    main()
