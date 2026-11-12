# Build the branded Windows wizard

Install Inno Setup 6.3 or newer, then configure and build:

```powershell
cmake -S . -B build -DBUILD_GUI=ON
cmake --build build --config Release --target windows_installer --parallel
```

If the compiler is not found automatically, set `-DISCC_EXECUTABLE="C:/path/to/ISCC.exe"` during configuration. The output is `build/installer/sattrak-VERSION-windows-x64-setup.exe`, using `SATTRAK_RELEASE_VERSION` for `VERSION`.

CMake stages only the application, bundled assets, texture, license, terms, and detected release MSVC runtime DLLs. Preferences, developer tools, and test outputs are not staged. A portable runtime can also be staged with `cmake --install build --config Release --component Runtime --prefix build/portable`.

The wizard installs for the current user into `%LOCALAPPDATA%/Programs/sattrak`, without requiring administrator privileges. Shortcuts use the application directory as their working directory. Users can optionally create a desktop shortcut and launch the app after setup. Uninstall removes the installed files and shortcuts, preserving user-created files such as `local/recordings` and preferences.

Interactive setup plays **2003 Toyota Corolla - Toyota Corolla**, the same permitted recording shipped with the app. It loops at 25% volume with a **Mute / Unmute** button on the bottom-left button strip. Muting leaves playback advancing; it only changes this player's volume. Playback stops when the wizard exits, including cancellation. Silent setup never starts audio. A hidden helper uses the app's bundled miniaudio decoder and static MSVC runtime, avoiding Windows MP3 codec dependencies. Named events control mute and shutdown across the wizard/helper; the helper also exits if the wizard process disappears. Audio-device or extraction failures disable the music button without blocking installation. Temporary audio files are extracted before the main payload and removed automatically by Setup. Existing [music credits and permission](../../assets/music/CREDITS.md) apply.

To exercise real decoding/output initialization, mute/unmute, shutdown and missing-file handling on a Windows machine with an audio device, run:

```powershell
./packaging/windows/test-installer-audio.ps1 -Helper ./build/Release/sattrak_installer_audio.exe -Track './assets/music/2003 Toyota Corolla.mp3'
```

This layout supports current relative resource/preference paths through a writable installation directory. Protected locations such as Program Files are unsupported until writable user data is separated from installation files.

Test a release on clean Windows without Visual Studio, including paths with spaces/non-ASCII characters, first/offline launch, upgrade, uninstall and reinstall. Local installer smoke tests do not replace clean-system validation. Public signing is not configured; the generated installer is unsigned.

See [branding provenance](../../docs/BRANDING.md) for the real screenshot sources.
