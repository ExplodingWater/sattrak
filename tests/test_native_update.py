#!/usr/bin/env python3
"""Isolated native installer/updater integration tests; no real installation/network."""
import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import struct
import tarfile
import tempfile
import unittest
from unittest import mock

SOURCE = Path(__file__).resolve().parents[1]
HELPER = SOURCE / 'packaging/native/native_updater.py'
spec = importlib.util.spec_from_file_location('native_updater', HELPER)
updater = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)


class NativeUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='sattrak-updater-test-')
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name).resolve()
        self.root = self.base / 'user folder with spaces' / 'sattrak'
        self.platform = updater.platform_id()

    def package(self, release='1.0.1', platform=None, extra=(), binary=None):
        path = self.base / (release + '-' + str(len(list(self.base.glob('*.tar.gz')))) + '.tar.gz')
        files = {
            'ag': b'#!/bin/sh\nprintf "%s\\n" "$PWD" "$@"\n',
            'native_updater.py': HELPER.read_bytes(),
            'release-version.txt': (release + '\n').encode(),
            'platform.txt': ((platform or self.platform) + '\n').encode(),
            'earth_texture.jpg': b'fixture image', 'LICENSE': b'MIT', 'TERMS.md': b'fixture terms',
            'assets/sky/stars.txt': b'fixture sky',
        }
        if binary:
            files['ag'] = Path(binary).read_bytes()
        with tarfile.open(path, 'w:gz') as archive:
            for name, data in files.items():
                info = tarfile.TarInfo('sattrak/' + name)
                info.size = len(data)
                archive.addfile(info, io.BytesIO(data))
            for name, kind, link in extra:
                info = tarfile.TarInfo(name)
                info.type = kind
                info.linkname = link
                if kind == tarfile.REGTYPE:
                    info.size = 1
                    archive.addfile(info, io.BytesIO(b'x'))
                else:
                    archive.addfile(info)
        return path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size

    def install(self, **kwargs):
        archive, digest, size = self.package('1.0.0', **kwargs)
        updater.install(archive, self.root, digest, size)
        return self.root

    def staged(self, release='1.0.1', **kwargs):
        archive, digest, size = self.package(release, **kwargs)
        staged = self.root / 'user-data/updates' / archive.name
        archive.rename(staged)
        return staged, digest, size

    def current(self):
        return (self.root / 'current/release-version.txt').read_text().strip()

    def test_install_launch_and_state_preservation(self):
        self.install()
        state = self.root / 'user-data/local'
        state.mkdir()
        (state / 'settings.txt').write_text('volume=75')
        (state / 'orbit.csv').write_text('user recording')
        old = (self.root / 'current').resolve()
        archive, digest, size = self.staged()
        updater.apply_update(archive, self.root, digest, size, 'v1.0.1')
        self.assertEqual(self.current(), '1.0.1')
        self.assertTrue(old.is_dir(), 'Retain previous release for rollback')
        self.assertFalse(archive.exists(), 'Remove successful staged download')
        self.assertEqual((state / 'settings.txt').read_text(), 'volume=75')
        self.assertEqual((state / 'orbit.csv').read_text(), 'user recording')
        self.assertTrue((self.root / 'user-data/assets/sky/stars.txt').is_file())
        launch = subprocess.run([self.root / 'sattrak', 'argument with spaces'], capture_output=True, text=True, timeout=5)
        self.assertEqual(launch.returncode, 0, launch.stderr)
        self.assertEqual(launch.stdout.splitlines(), [str(self.root / 'user-data'), 'argument with spaces'])

    def test_corrupt_download_does_not_change_active_release(self):
        self.install()
        archive, digest, size = self.staged()
        with archive.open('ab') as file:
            file.write(b'corrupt')
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.0')
        with self.assertRaisesRegex(ValueError, 'SHA-256'):
            updater.apply_update(archive, self.root, digest, archive.stat().st_size, '1.0.1')

    def test_reject_downgrade_same_version_and_wrong_tag(self):
        self.install()
        for release in ('0.9.0', '1.0.0', '1.0.0+rebuilt', '1.0.0-preview.1'):
            archive, digest, size = self.staged(release)
            with self.assertRaisesRegex(ValueError, 'current'):
                updater.apply_update(archive, self.root, digest, size, release)
        archive, digest, size = self.staged('1.0.2')
        with self.assertRaisesRegex(ValueError, 'does not match'):
            updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.0')

    def test_other_platform_rejected(self):
        self.install()
        archive, digest, size = self.staged(platform='not-this-platform')
        with self.assertRaisesRegex(ValueError, 'operating system or architecture'):
            updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.0')

    def test_archive_path_and_link_attacks(self):
        self.install()
        attacks = [
            ('sattrak/../../escape', tarfile.REGTYPE, ''),
            ('/tmp/escape', tarfile.REGTYPE, ''),
            ('sattrak/assets/link', tarfile.SYMTYPE, '/tmp'),
            ('sattrak/assets/link', tarfile.LNKTYPE, 'sattrak/ag'),
            ('sattrak/assets/device', tarfile.CHRTYPE, ''),
            ('sattrak/user-data/overwrite', tarfile.REGTYPE, ''),
            ('sattrak/assets/..\\escape', tarfile.REGTYPE, ''),
            ('sattrak/ag', tarfile.REGTYPE, ''),
        ]
        for attack in attacks:
            with self.subTest(attack=attack):
                archive, digest, size = self.staged(extra=[attack])
                with self.assertRaisesRegex(ValueError, 'archive member'):
                    updater.apply_update(archive, self.root, digest, size, '1.0.1')
                self.assertEqual(self.current(), '1.0.0')
                self.assertFalse(list((self.root / 'releases').glob('.stage-*')))

    def test_session_lock_postpones_install(self):
        self.install()
        archive, digest, size = self.staged()
        # Shorten only the timeout; use the actual independent file lock.
        original = updater.installation_lock
        with original(self.root, exclusive=False):
            with mock.patch.object(updater, 'installation_lock', side_effect=lambda root, **kwargs: original(root)):
                with self.assertRaisesRegex(RuntimeError, 'another sattrak session'):
                    updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.0')
        updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.1')

    def test_failed_atomic_switch_rolls_back(self):
        self.install()
        archive, digest, size = self.staged()
        before = set((self.root / 'releases').iterdir())
        with mock.patch.object(updater, 'switch_current', side_effect=OSError('simulated rename failure')):
            with self.assertRaises(OSError):
                updater.apply_update(archive, self.root, digest, size, '1.0.1')
        self.assertEqual(self.current(), '1.0.0')
        self.assertEqual(set((self.root / 'releases').iterdir()), before)

    def test_source_tree_and_foreign_download_untouched(self):
        self.install()
        archive, digest, size = self.package()
        with self.assertRaisesRegex(ValueError, 'installation cache'):
            updater.apply_update(archive, self.root, digest, size, '1.0.1')
        source = self.base / 'source'
        source.mkdir()
        (source / 'CMakeLists.txt').write_text('user project')
        with self.assertRaisesRegex(ValueError, 'empty'):
            updater.install(archive, source, digest, size)
        self.assertEqual((source / 'CMakeLists.txt').read_text(), 'user project')
        with self.assertRaises(FileNotFoundError):
            updater.managed_root(source)

    def test_verify_rejects_symlink(self):
        archive, digest, size = self.package()
        link = self.base / 'symlink'
        link.symlink_to(archive)
        with self.assertRaises(OSError):
            with updater.verified_archive(link, digest, size):
                pass

    @unittest.skipIf(os.geteuid() == 0, 'The real CLI intentionally rejects root installs')
    def test_cli_waits_for_parent_then_applies(self):
        self.install()
        archive, digest, size = self.staged()
        parent = subprocess.Popen(['python3', '-c', 'import time; time.sleep(0.3)'])
        worker = subprocess.Popen(['python3', str(HELPER), '--apply', str(archive),
            '--root', str(self.root), '--sha256', digest, '--size', str(size),
            '--tag', 'v1.0.1', '--parent', str(parent.pid)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            self.assertEqual(self.current(), '1.0.0')
            parent.wait(timeout=5)
            output, error = worker.communicate(timeout=5)
            self.assertEqual(worker.returncode, 0, output + error)
            self.assertEqual(self.current(), '1.0.1')
            self.assertIn('Update installed', (self.root / 'user-data/updates/result.txt').read_text())
        finally:
            if parent.poll() is None:
                parent.kill()
            parent.wait()
            if worker.poll() is None:
                worker.kill()
                worker.wait()

    def test_version_ordering_matches_release_checker(self):
        for newer, older in [('1.10.0', '1.9.9'), ('1.0.0', '1.0.0-preview.1'), ('1.0.0-preview.10', '1.0.0-preview.9')]:
            self.assertGreater(updater.version(newer), updater.version(older))
        self.assertEqual(updater.version('v1.0.0+new'), updater.version('1.0.0+old'))
        for invalid in ('1.0.0-01', '01.0.0', '1.0.0/evil', 'v1.0.0\n'):
            with self.assertRaises(ValueError):
                updater.version(invalid)

    def test_package_architecture_comes_from_binary_not_python(self):
        fixture = self.base / 'executable'
        for cpu, expected in ((0x0100000c, 'macos-arm64'), (0x01000007, 'macos-x64')):
            header = bytearray(64)
            header[:4] = b'\xcf\xfa\xed\xfe'
            struct.pack_into('<I', header, 4, cpu)
            fixture.write_bytes(header)
            self.assertEqual(updater.binary_platform(fixture), expected)
        for cpu, expected in ((183, 'debian-arm64'), (62, 'debian-amd64')):
            header = bytearray(64)
            header[:6] = b'\x7fELF\x02\x01'
            struct.pack_into('<H', header, 18, cpu)
            fixture.write_bytes(header)
            self.assertEqual(updater.binary_platform(fixture), expected)
        fixture.write_bytes(b'\xca\xfe\xba\xbe' + bytes(60))
        with self.assertRaisesRegex(ValueError, 'single-architecture'):
            updater.binary_platform(fixture)

    @unittest.skipUnless(os.environ.get('SATTRAK_NATIVE_FIXTURE'), 'Run through CTest to include the C++ fixture')
    @unittest.skipIf(os.geteuid() == 0, 'Native runtime deliberately rejects root installations')
    def test_cpp_runtime_and_shutdown_helper_integration(self):
        fixture = Path(os.environ['SATTRAK_NATIVE_FIXTURE'])
        self.platform = updater.binary_platform(fixture)
        # CMake may itself be Intel under Rosetta; fixture architecture is the
        # real app architecture. The spawned native helper detects hardware.
        with mock.patch.object(updater, 'platform_id', return_value=self.platform):
            self.install(binary=fixture)
            archive, digest, size = self.staged(binary=fixture)
        previous_binary = (self.root / 'current/ag').resolve()
        probe = subprocess.run([self.root / 'sattrak'], capture_output=True, text=True, timeout=5)
        self.assertEqual(probe.returncode, 0, probe.stderr)
        self.assertEqual(probe.stdout.splitlines(), [str(self.root), str(self.root / 'user-data')])
        scheduled = subprocess.run([self.root / 'sattrak', '--schedule', str(archive), digest, str(size), 'v1.0.1'],
                                   capture_output=True, text=True, timeout=5)
        self.assertEqual(scheduled.returncode, 0, scheduled.stderr)
        import time
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and self.current() != '1.0.1':
            time.sleep(0.05)
        log = self.root / 'user-data/updates/update-install.log'
        self.assertEqual(self.current(), '1.0.1', log.read_text() if log.exists() else 'Updater did not start')
        inactive = subprocess.run([previous_binary], capture_output=True, text=True, timeout=5)
        self.assertEqual(inactive.returncode, 1)
        self.assertIn('start sattrak using its launcher', inactive.stderr)


if __name__ == '__main__':
    unittest.main()
