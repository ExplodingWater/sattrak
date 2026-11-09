# App updates

sattrak checks published GitHub releases once at launch and downloads a newer Windows installer in the background when automatic updates are enabled

The installed Windows app applies a verified update after normal shutdown, keeping the current session open until you choose to close it

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

Automatic installation currently applies to the per-user Windows installation registered by the sattrak setup wizard

Portable builds and macOS/Linux continue to offer a release link

## Automatic installation

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

The current source baseline is `1.0.0-preview.20261103`

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

Windows checks also cover SHA-256 verification, incomplete or modified files and write-lock protection

The isolated integration test uses its own registry key and cache folder to verify parent-process waiting, silent replacement, recording preservation and same-version rejection

```powershell
cmake --build build --config Release --target sattrak_updater_fixture
./tests/test-updater-install.ps1 -Helper ./build/Release/sattrak_updater_fixture.exe -Iscc "C:/path/to/ISCC.exe" -WorkFolder ./build/updater-fixtures
```

Use `ag.exe --download-update-test` to download and verify the current official Windows installer without installing it

Use the native network check without opening the GUI

```sh
./ag --check-updates
```

On Windows use `ag.exe --check-updates`

The graphical smoke tests skip automatic network checks to remain deterministic
