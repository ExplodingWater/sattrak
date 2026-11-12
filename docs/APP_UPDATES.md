# App updates

sattrak checks published GitHub releases once at launch and downloads a newer matching package in the background when automatic updates are enabled.

Managed installations apply a verified update after normal shutdown, keeping the current session open until you choose to close it

Use **Settings → General → App updates → Install updates automatically when closing sattrak** to change this behavior

Install the rebuilt setup once to get the updater helper and version marker, since older installations and portable builds do not contain them

Use **Settings → General → App updates → Check now** to retry

- **View release and downloads** opens the release page in your browser
- **Later** dismisses the notice for the current session
- Dismissing a notice does not turn off automatic updates
- Failed checks stay quiet and remain visible as a status in Settings
- Startup and shutdown continue without waiting for the network
- The tutorial and existing dialogs take priority over update notices

The check runs independently of the satellite catalog choice and sends an HTTPS request to the public sattrak release list without authentication or local files

Windows uses WinHTTP and macOS/Linux use libcurl with certificate verification enabled

Automatic installation supports the registered per-user Windows installation and
managed per-user macOS/Debian installations. Unmanaged portable/source builds,
Homebrew and unmanaged system installations offer a release link. The official
[desktop installers](INSTALLERS.md) supply a native launcher that prepares this same
per-user layout. Neither app updates nor the launcher rewrite system-owned app
files. Debian package upgrades refresh the bootstrap payload through apt/dpkg;
per-user app updates operate separately after launch.

## macOS and Debian native installations

Native packages require **Python 3.9+** on PATH at runtime. The release workflow
builds macOS 13+ packages for Apple Silicon and Intel, and Debian 12+ packages for
amd64 and arm64. Debian also needs its normal OpenGL/X11 runtime libraries and
`libcurl4`; audio uses the installed audio backend. macOS uses the system libcurl.
The standalone update archives use a terminal launcher. Initial installation can
also use the [Finder app/DMG/PKG or Debian package](INSTALLERS.md). Developer ID
signing and notarization require your release credentials; no Gatekeeper
quarantine attributes are removed.

Build the package locally, using the release version you intend to publish:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSATTRAK_RELEASE_VERSION=1.0.1
cmake --build build --parallel 4
cmake --build build --target native_package
```

The output under `build/packages/` contains the platform archive, a `.sha256`
file, and `native_updater.py` for the first installation. For example, on Apple
Silicon (replace the filename with `debian-amd64`, `debian-arm64`, or `macos-x64`
as appropriate):

```sh
python3 build/packages/native_updater.py \
  --install build/packages/sattrak-1.0.1-macos-arm64.tar.gz \
  --sha256 "$(cut -d ' ' -f 1 build/packages/sattrak-1.0.1-macos-arm64.tar.gz.sha256)" \
  --root "$HOME/Applications/sattrak"
"$HOME/Applications/sattrak/sattrak"
```

On Debian a suitable root is `$HOME/.local/opt/sattrak`. Choose an empty folder
owned by your normal account. Do not run the installer or app with `sudo`.
For downloaded packages, obtain the initial digest and bootstrap helper from the
trusted official release. The automatic path obtains digest and size directly
from GitHub's HTTPS release API; the checksum is not a separate publisher signature.

Installation layout:

- `sattrak`: launcher that locks the installation before choosing the active release.
- `current`: atomically switched symlink to a complete directory under `releases/`.
- `releases/`: immutable app/assets payloads; the previous release stays for recovery.
- `user-data/`: preferences, recordings and other mutable files. Asset links resolve
  through `current`, keeping existing relative asset paths working after an update.
- `user-data/updates/`: staged downloads, automatic-update preference, result and log.

Updates select an exact OS/architecture/version asset, require the GitHub SHA-256
and size, and download through HTTPS only. The helper verifies again after exit,
rejects traversal, duplicate members, links and special files, limits unpacked size,
and checks internal version/platform markers. It waits for the closing process,
postpones if another session holds a lock, extracts into a new release directory,
and atomically switches `current` only when extraction and validation succeed.
Failed extraction or switching leaves the active release unchanged. The helper
never requests elevated privileges, alters the source checkout, or overwrites
`user-data`. It does not force-quit sessions or automatically restart the app.

Old release directories are retained deliberately; remove unused releases only
while the app is closed and keep the active `current` target. To roll back, close
all sessions and point `current` at the previous complete release. Turn off
automatic installation first if you want to keep using the older version.

## Publishing native update assets

Run **Native update packages** (`.github/workflows/native-packages.yml`) with the
intended semantic version. It builds the Windows x64 installer and the Mac and
Debian installers and update archives, then uploads them for review. Debian
jobs check the installed package on Debian 12 with a virtual display and audio sink.

Upload the corresponding archives to the official GitHub release:

```text
sattrak-1.0.1-macos-arm64.tar.gz
sattrak-1.0.1-macos-x64.tar.gz
sattrak-1.0.1-debian-amd64.tar.gz
sattrak-1.0.1-debian-arm64.tar.gz
```

Also publish the checksum files and one copy of `native_updater.py` for initial
installation. Asset filenames and internal version markers must match the tag
(without the optional leading `v`). GitHub must expose a `sha256:` asset digest;
if it is absent or there is no matching platform asset, the app offers the release
link without downloading an incompatible package. Upload all platform assets
before publishing the release so clients can see a complete set.

## Windows automatic installation

- Download only the official release's Windows x64 setup asset
- Require GitHub's SHA-256 digest and exact file size before staging
- Check the digest again while locking the installer against changes
- Wait for the current app process to exit
- Confirm the registered installation folder and reject a downgrade or repeated update
- Postpone installation if another sattrak session is running
- Install quietly without forcing applications closed or restarting Windows
- Preserve user-created preferences and recordings

The downloaded installer is removed after success, with status and logs under `%LOCALAPPDATA%\sattrak\updates`

Failed downloads offer manual retry and never launch a partial package

Turning automatic installation off also prevents a staged update from running when the app closes

## Release versions

The current source baseline is `1.0.0-preview.20261109.1`

Set `SATTRAK_RELEASE_VERSION` to match the release tag when building a new release

```sh
cmake -S . -B build -DSATTRAK_RELEASE_VERSION=1.0.1
```

Use semantic tags such as `v1.0.1` or `v1.1.0-preview.1`

Stable builds consider only stable releases while preview builds also consider previews, with numeric version comparison preventing downgrades and repeated notices for the installed version

The checker examines the 30 most recent published releases and ignores drafts, unsupported tags and build metadata differences

Normal source builds retain the configured release baseline until it is changed, so a newly published build must set its version before packaging

## Verification

CTest covers semantic version ordering, preview channels, malformed responses, trusted release URLs, worker responsiveness, duplicate checks, network failures and shutdown

Windows checks also cover SHA-256 verification, incomplete or modified files and write-lock protection. Native tests use isolated temporary installations to cover initial launch, state preservation, corrupt packages, architecture/version mismatches, malicious archive paths and links, live-session locking, parent waiting, and rollback after a failed pointer switch.

The isolated integration test uses its own registry key and cache folder to verify parent-process waiting, silent replacement, recording preservation and same-version rejection

```powershell
cmake --build build --config Release --target sattrak_updater_fixture
./tests/test-updater-install.ps1 -Helper ./build/Release/sattrak_updater_fixture.exe -Iscc "C:/path/to/ISCC.exe" -WorkFolder ./build/updater-fixtures
```

Use `ag.exe --download-update-test` (Windows) or `./ag --download-update-test` (native) to download and verify a matching official package without installing it. It reports an unavailable package if no compatible native asset has been published.

Use the native network check without opening the GUI

```sh
./ag --check-updates
```

On Windows use `ag.exe --check-updates`

Use `./ag --native-install-info` on macOS/Debian to inspect the compiled platform,
managed installation root and working directory without opening the GUI.

The graphical smoke tests skip automatic network checks to remain deterministic
