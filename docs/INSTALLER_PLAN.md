# Windows installation wizard

The Windows wizard is implemented in [packaging/windows/sattrak.iss](../packaging/windows/sattrak.iss), with captured application artwork, shortcuts, uninstall support, and a CMake `windows_installer` build target. See the [build and verification guide](../packaging/windows/README.md)

## Packaging prerequisites

- Separate read-only bundled resources from writable user data. main_gui.cpp currently changes the working directory to the executable directory; panel layout and other preferences use relative paths. Store preferences, caches and recordings under a dedicated user data directory before supporting installation under Program Files.
- Package ag.exe, earth_texture.jpg, assets, LICENSE and TERMS.md, matching the existing CMake post-build copies. Audit optional audio and catalog assets for distribution rights and size before choosing what ships.
- CMake stages the detected release MSVC runtime DLLs. Test on a clean Windows machine without Visual Studio before public distribution.

## Wizard behavior

The per-user wizard provides a welcome screen, license and terms, install location, optional desktop shortcut, installation progress and a finish page with optional launch. It installs to `%LOCALAPPDATA%/Programs/sattrak` by default without requiring administrator privileges. Shortcuts use the application directory as their working directory. All-users installation is deferred until the user-data migration is complete.

## Release acceptance

Verify paths with spaces and non-ASCII characters, first launch, offline launch, upgrade, uninstall, and reinstall. Preserve user preferences and recordings during upgrades and uninstall unless the user explicitly requests their removal. Test bundled-resource discovery when launching from a shortcut or a different working directory. Sign release binaries and the installer before public distribution when signing credentials are available.

The CMake target stages the runtime payload and detected release MSVC DLLs, then compiles the setup executable under `build/installer`. Uninstall preserves user-created recordings and preferences. The setup executable is unsigned; clean-system and upgrade validation remain required before public release. Artwork provenance is documented in [BRANDING.md](BRANDING.md)
