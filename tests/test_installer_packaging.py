#!/usr/bin/env python3
"""Validate installer layout/metadata without installing into the host system."""
import importlib.util
from pathlib import Path
import plistlib
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest

SOURCE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SOURCE / 'scripts'))
import package_installers as installers


class InstallerPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='sattrak-packaging-test-')
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.source = self.base / 'source'
        (self.source / 'assets/branding').mkdir(parents=True)
        (self.source / 'packaging/native').mkdir(parents=True)
        (self.source / 'packaging/native/native_updater.py').write_bytes((SOURCE / 'packaging/native/native_updater.py').read_bytes())
        (self.source / 'assets/branding/sattrak.png').write_bytes((SOURCE / 'assets/branding/sattrak.png').read_bytes())
        for name in ('LICENSE', 'TERMS.md', 'earth_texture.jpg'):
            (self.source / name).write_text('fixture data')
        for library, name in (('glfw', 'LICENSE.md'), ('imgui', 'LICENSE.txt'), ('implot', 'LICENSE'),
                              ('miniaudio', 'LICENSE'), ('sgp4', 'LICENSE'), ('nlohmann', 'LICENSE.MIT')):
            folder = self.source / 'external' / library
            folder.mkdir(parents=True)
            (folder / name).write_text('fixture notice')
        self.binary = self.base / 'ag'
        header = bytearray(64)
        header[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', header, 18, 62)
        self.binary.write_bytes(header)
        self.launcher = self.base / 'launcher'
        self.launcher.write_bytes(header)
        self.args = SimpleNamespace(binary=self.binary, launcher=self.launcher, source=self.source, version='1.0.0-preview.20261109')

    def test_debian_tree_uses_system_payload_and_native_launcher(self):
        root = self.base / 'package'
        installers.prepare_debian_tree(self.args, 'debian-amd64', root, 'libc6 (>= 2.34), libstdc++6 (>= 12)')
        self.assertEqual((root / 'usr/bin/sattrak').read_bytes(), self.launcher.read_bytes())
        self.assertEqual((root / 'usr/bin/sattrak').stat().st_mode & 0o777, 0o755)
        payload = root / 'usr/lib/sattrak/payload'
        self.assertEqual((payload / 'platform.txt').read_text().strip(), 'debian-amd64')
        self.assertEqual((payload / 'release-version.txt').read_text().strip(), self.args.version)
        self.assertTrue((payload / 'assets/third-party-licenses/glfw-LICENSE.md').is_file())
        control = (root / 'DEBIAN/control').read_text()
        self.assertIn('Architecture: amd64\n', control)
        self.assertIn('Version: 1.0.0~preview.20261109-1\n', control)
        self.assertIn('python3 (>= 3.9)', control)
        self.assertIn('libc6 (>= 2.34)', control)
        self.assertFalse((root / 'home').exists(), 'Installers must not write into home directories')
        self.assertTrue((root / 'usr/share/icons/hicolor/256x256/apps/sattrak.png').is_file())
        self.assertIn('Exec=sattrak\n', (root / 'usr/share/applications/sattrak.desktop').read_text())
        self.assertFalse((root / 'DEBIAN/postinst').exists(), 'No privileged scripts modify per-user state')

    def test_debian_version_mapping(self):
        self.assertEqual(installers.debian_version('1.2.3'), '1.2.3-1')
        self.assertEqual(installers.debian_version('1.2.3-preview.2'), '1.2.3~preview.2-1')
        self.assertEqual(installers.debian_version('1.2.3-preview.2+build.4'), '1.2.3~preview.2+build.4-1')

    def test_mac_bundle_metadata(self):
        info = installers.app_info(self.args.version, 'macos-arm64')
        serialized = plistlib.dumps(info)
        restored = plistlib.loads(serialized)
        self.assertEqual(restored['CFBundleExecutable'], 'sattrak')
        self.assertEqual(restored['CFBundleIdentifier'], 'com.explodingwater.sattrak')
        self.assertEqual(restored['CFBundlePackageType'], 'APPL')
        self.assertEqual(restored['CFBundleShortVersionString'], '1.0.0')
        self.assertIn(self.args.version, restored['CFBundleGetInfoString'])
        self.assertTrue(restored['NSHighResolutionCapable'])
        self.assertEqual(restored['CFBundleIconFile'], 'sattrak.icns')

    def test_desktop_entry_has_no_shell_or_elevation(self):
        entry = installers.desktop_entry()
        self.assertIn('Terminal=false\n', entry)
        self.assertIn('StartupWMClass=sattrak\n', entry)
        self.assertNotIn('sudo', entry)
        self.assertNotIn('sh -c', entry)


if __name__ == '__main__':
    unittest.main()
