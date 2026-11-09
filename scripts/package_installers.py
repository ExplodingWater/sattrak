#!/usr/bin/env python3
"""Build genuine macOS .app/.dmg/.pkg or Debian .deb installers.

Use the CMake macos_installer or debian_installer target on the target OS.
"""
import argparse
import html
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from package_native import checksum, load_updater, stage_payload

IDENTIFIER = 'com.explodingwater.sattrak'


def run(arguments, **kwargs):
    print('+', ' '.join(map(str, arguments)), flush=True)
    try:
        return subprocess.run(list(map(str, arguments)), check=True, **kwargs)
    except subprocess.CalledProcessError as error:
        if error.stdout:
            print(error.stdout, file=sys.stderr)
        if error.stderr:
            print(error.stderr, file=sys.stderr)
        raise


def core_version(version):
    # Keep a real numeric CFBundle version, with the full semantic release in
    # payload/release-version.txt and CFBundleGetInfoString.
    return '.'.join(re.match(r'([0-9]+)\.([0-9]+)\.([0-9]+)', version).groups())


def debian_version(version):
    base, separator, metadata = version.partition('+')
    core, preview_separator, preview = base.partition('-')
    return core + ('~' + preview if preview_separator else '') + ('+' + metadata if separator else '') + '-1'


def desktop_entry():
    return '''[Desktop Entry]
Version=1.0
Type=Application
Name=sattrak
Comment=Explore Earth, spacecraft and orbital dynamics
Exec=sattrak
Icon=sattrak
Terminal=false
StartupNotify=false
StartupWMClass=sattrak
Categories=Education;Science;Astronomy;
Keywords=satellite;orbit;space;astronomy;
'''


def app_info(version, platform, minimum_os="13.0"):
    return {
        'CFBundleDevelopmentRegion': 'en', 'CFBundleDisplayName': 'sattrak',
        'CFBundleExecutable': 'sattrak', 'CFBundleIdentifier': IDENTIFIER,
        'CFBundleInfoDictionaryVersion': '6.0', 'CFBundleName': 'sattrak',
        'CFBundlePackageType': 'APPL', 'CFBundleIconFile': 'sattrak.icns',
        'CFBundleShortVersionString': core_version(version), 'CFBundleVersion': core_version(version),
        'CFBundleGetInfoString': 'sattrak ' + version + ' (' + platform + ')',
        'LSMinimumSystemVersion': minimum_os, 'LSApplicationCategoryType': 'public.app-category.education',
        'NSHighResolutionCapable': True, 'NSHumanReadableCopyright': 'Martin Hafizi. MIT license; asset credits included.',
    }


def make_icns(png, output, temporary):
    iconset = temporary / 'sattrak.iconset'
    iconset.mkdir()
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            name = f'icon_{size}x{size}' + ('@2x' if scale == 2 else '') + '.png'
            run(['/usr/bin/sips', '-z', size * scale, size * scale, png, '--out', iconset / name], stdout=subprocess.DEVNULL)
    run(['/usr/bin/iconutil', '-c', 'icns', iconset, '-o', output])


def normalize_permissions(root):
    for path in (root, *root.rglob('*')):
        if path.is_symlink():
            continue
        executable = path.is_file() and (path.name == 'ag' or path.name == 'sattrak' and path.parent.name in ('bin', 'MacOS'))
        path.chmod(0o755 if path.is_dir() or executable else 0o644)


def minimum_macos(binary):
    output = subprocess.check_output(['/usr/bin/otool', '-l', str(binary)], text=True)
    match = re.search(r'\bminos\s+([0-9.]+)', output)
    if not match:
        match = re.search(r'LC_VERSION_MIN_MACOSX.*?\bversion\s+([0-9.]+)', output, re.S)
    if not match:
        raise ValueError('Cannot determine the macOS deployment target')
    return match.group(1)


def macos(args, native_platform):
    if sys.platform != 'darwin' or not native_platform.startswith('macos-'):
        raise ValueError('Build Mac installers on macOS with a Mach-O application')
    stem = f'sattrak-{args.version}-{native_platform}'
    with tempfile.TemporaryDirectory(prefix='.macos-installer-', dir=args.output) as folder:
        temporary = Path(folder)
        app = temporary / 'application/sattrak.app'
        contents = app / 'Contents'
        (contents / 'MacOS').mkdir(parents=True)
        (contents / 'Resources').mkdir()
        stage_payload(args.binary, args.source, args.version, contents / 'Resources/payload')
        shutil.copy2(args.launcher, contents / 'MacOS/sattrak')
        with (contents / 'Info.plist').open('wb') as out:
            minimum = max((minimum_macos(args.binary), minimum_macos(args.launcher)), key=lambda v: tuple(map(int, v.split('.'))))
            plistlib.dump(app_info(args.version, native_platform, minimum), out)
        (contents / 'PkgInfo').write_bytes(b'APPL????')
        make_icns(args.source / 'assets/branding/sattrak.png', contents / 'Resources/sattrak.icns', temporary)
        normalize_permissions(app)
        signing = ['--force', '--sign', args.app_signing_identity]
        if args.app_signing_identity != '-':
            signing += ['--options', 'runtime', '--timestamp']
        for path, identifier in ((contents / 'Resources/payload/ag', IDENTIFIER + '.engine'),
                                 (contents / 'MacOS/sattrak', IDENTIFIER + '.launcher'), (app, IDENTIFIER)):
            run(['/usr/bin/codesign', *signing, '--identifier', identifier, path])
        run(['/usr/bin/codesign', '--verify', '--deep', '--strict', app])

        # Real Installer.app product with welcome, license and installation pages.
        component = temporary / 'sattrak-component.pkg'
        components = temporary / 'components.plist'
        with components.open('wb') as out:
            plistlib.dump([{'RootRelativeBundlePath': 'sattrak.app', 'BundleIsRelocatable': False,
                           'BundleIsVersionChecked': False, 'BundleHasStrictIdentifier': True,
                           'BundleOverwriteAction': 'upgrade'}], out)
        run(['/usr/bin/pkgbuild', '--root', app.parent, '--install-location', '/Applications',
             '--component-plist', components, '--identifier', IDENTIFIER, '--version', core_version(args.version), component])
        resources = temporary / 'installer-resources'
        resources.mkdir()
        (resources / 'Welcome.html').write_text('<html><body><h1>sattrak</h1><p>Explore Earth from orbit.</p><p>Version ' + html.escape(args.version) + '</p></body></html>')
        (resources / 'ReadMe.html').write_text('''<html><body><h2>Getting started</h2>
<p>sattrak installs in Applications. Open it from Finder after installation.</p>
<p>Your account keeps its recordings, preferences and complete active release under
Library/Application Support/sattrak/installation. Installing a newer app preserves this data.</p>
<h2>Automatic updates</h2><p>The app itself runs without Python. The automatic-update helper
requires Python 3.9 or newer on PATH. Without it, use the release link in Settings to update manually.</p>
<p>Spacecraft models, music, radio recordings and offline catalogs are included.</p></body></html>''')
        shutil.copy2(args.source / 'LICENSE', resources / 'License.txt')
        distribution = ET.Element('installer-gui-script', minSpecVersion='2')
        ET.SubElement(distribution, 'title').text = 'sattrak'
        for node, filename in (('welcome', 'Welcome.html'), ('readme', 'ReadMe.html'), ('license', 'License.txt')):
            ET.SubElement(distribution, node, file=filename)
        ET.SubElement(distribution, 'options', {'require-scripts': 'false'}, customize='never', rootVolumeOnly='true',
                      hostArchitectures='arm64' if native_platform.endswith('arm64') else 'x86_64')
        ET.SubElement(distribution, 'domains', enable_anywhere='false', enable_currentUserHome='false', enable_localSystem='true')
        outline = ET.SubElement(distribution, 'choices-outline')
        ET.SubElement(outline, 'line', choice='sattrak')
        choice = ET.SubElement(distribution, 'choice', id='sattrak', title='sattrak')
        ET.SubElement(choice, 'pkg-ref', id=IDENTIFIER)
        ET.SubElement(distribution, 'pkg-ref', id=IDENTIFIER, version=core_version(args.version)).text = component.name
        ET.ElementTree(distribution).write(temporary / 'Distribution.xml', encoding='utf-8', xml_declaration=True)
        pkg = args.output / (stem + '.pkg')
        command = ['/usr/bin/productbuild', '--distribution', temporary / 'Distribution.xml',
                   '--resources', resources, '--package-path', temporary]
        if args.installer_signing_identity:
            command += ['--sign', args.installer_signing_identity]
        run([*command, pkg])
        checksum(pkg)

        # Keep the built .app available locally and include it in a drag-install DMG.
        final_app = args.output / 'sattrak.app'
        if final_app.exists():
            shutil.rmtree(final_app)
        shutil.copytree(app, final_app)
        disk = temporary / 'disk'
        disk.mkdir()
        shutil.copytree(app, disk / 'sattrak.app')
        (disk / 'Applications').symlink_to('/Applications', target_is_directory=True)
        (disk / 'Read Me.txt').write_text('Drag sattrak.app to Applications, then open it.\nRecordings and settings stay in your user Library.\nAutomatic updates require Python 3.9+.\n')
        dmg = args.output / (stem + '.dmg')
        run(['/usr/bin/hdiutil', 'create', '-volname', 'sattrak', '-srcfolder', disk, '-fs', 'HFS+', '-format', 'UDZO', '-ov', dmg])
        run(['/usr/bin/hdiutil', 'verify', dmg])
        checksum(dmg)
        print(pkg)
        print(dmg)


def debian_control(version, architecture, dependencies, size):
    return f'''Package: sattrak
Version: {debian_version(version)}
Section: science
Priority: optional
Architecture: {architecture}
Maintainer: Martin Hafizi <hafizi.martin@gmail.com>
Installed-Size: {size}
Depends: python3 (>= 3.9), ca-certificates, {dependencies}
Recommends: libasound2 | libasound2t64
Homepage: https://github.com/ExplodingWater/sattrak
Description: Desktop satellite and orbital dynamics simulator
 Explore a three-dimensional Earth, spacecraft and orbital trajectories.
 Includes offline catalogs, spacecraft models, constellation guides,
 audio and a guided tutorial. Each user keeps settings and recordings
 separately from system-owned files, with verified per-user app updates.
'''


def prepare_debian_tree(args, native_platform, root, dependencies):
    architecture = {'debian-amd64': 'amd64', 'debian-arm64': 'arm64'}[native_platform]
    payload = root / 'usr/lib/sattrak/payload'
    stage_payload(args.binary, args.source, args.version, payload)
    (root / 'usr/bin').mkdir(parents=True)
    shutil.copy2(args.launcher, root / 'usr/bin/sattrak')
    applications = root / 'usr/share/applications'
    applications.mkdir(parents=True)
    (applications / 'sattrak.desktop').write_text(desktop_entry())
    icons = root / 'usr/share/icons/hicolor/256x256/apps'
    icons.mkdir(parents=True)
    shutil.copy2(args.source / 'assets/branding/sattrak.png', icons / 'sattrak.png')
    docs = root / 'usr/share/doc/sattrak'
    docs.mkdir(parents=True)
    (docs / 'copyright').write_text('sattrak — Martin Hafizi\n\n' + (args.source / 'LICENSE').read_text() +
        '\nBundled library notices and asset credits are installed under /usr/lib/sattrak/payload/assets/.\n')
    (docs / 'README.Debian').write_text('''Launch sattrak from the application menu or run sattrak in a terminal.
The system package provides a read-only bootstrap payload. Each normal user
keeps a complete active release, recordings and settings under
${XDG_DATA_HOME:-$HOME/.local/share}/sattrak/installation.
The app never overwrites dpkg-managed files. Automatic app updates apply only
to this per-user installation after all sessions close. A newer Debian package
seeds its newer release on the next launch; older packages never downgrade an
already newer per-user release. Removing/purging the package preserves user data.
''')
    normalize_permissions(root)
    size = (sum(p.stat().st_size for p in root.rglob('*') if p.is_file()) + 1023) // 1024
    (root / 'DEBIAN').mkdir()
    (root / 'DEBIAN/control').write_text(debian_control(args.version, architecture, dependencies, size))
    (root / 'DEBIAN/control').chmod(0o644)


def debian(args, native_platform):
    if sys.platform != 'linux' or not native_platform.startswith('debian-'):
        raise ValueError('Build Debian installers on Debian/Ubuntu with ELF executables')
    for tool in ('dpkg-deb', 'dpkg-shlibdeps'):
        if not shutil.which(tool):
            raise ValueError(tool + ' is required (install dpkg-dev)')
    with tempfile.TemporaryDirectory(prefix='.debian-installer-', dir=args.output) as folder:
        temporary = Path(folder)
        (temporary / 'debian').mkdir()
        (temporary / 'debian/control').write_text('Source: sattrak\n\nPackage: sattrak\nArchitecture: any\n')
        output = run(['dpkg-shlibdeps', '-O', '-e' + str(args.binary), '-e' + str(args.launcher)],
                     cwd=temporary, capture_output=True, text=True)
        dependencies = next((line.split('=', 1)[1] for line in output.stdout.splitlines() if line.startswith('shlibs:Depends=')), '')
        if not dependencies:
            raise ValueError('Could not determine Debian shared-library dependencies')
        root = temporary / 'root'
        prepare_debian_tree(args, native_platform, root, dependencies)
        package = args.output / f'sattrak-{args.version}-{native_platform}.deb'
        run(['dpkg-deb', '--root-owner-group', '--build', root, package])
        run(['dpkg-deb', '--info', package])
        checksum(package)
        print(package)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'launcher', 'source', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--format', choices=('macos', 'debian'), required=True)
    parser.add_argument('--app-signing-identity', default='-')
    parser.add_argument('--installer-signing-identity', default='')
    args = parser.parse_args()
    for name in ('binary', 'launcher', 'source', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    updater = load_updater(args.source)
    updater.version(args.version)
    if args.version.startswith('v'):
        parser.error('Use a release version without a leading v')
    native_platform = updater.binary_platform(args.binary)
    if updater.binary_platform(args.launcher) != native_platform:
        parser.error('Launcher and application must have matching OS/architecture')
    args.output.mkdir(parents=True, exist_ok=True)
    (macos if args.format == 'macos' else debian)(args, native_platform)


if __name__ == '__main__':
    main()
