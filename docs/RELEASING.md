# Publish a Windows download on GitHub

The installation wizard is a **GitHub Release asset**, not a binary committed to the source repository. The README links directly to that asset, so visitors can download without building the project or using GitHub Actions.

1. Build and test the source revision to be released. On Windows, configure CMake with `ISCC_EXECUTABLE` pointing to Inno Setup's `ISCC.exe`.
2. Run `cmake --build build --config Release --target windows_installer --parallel`.
3. Run CTest, the GUI walkthrough smoke tests and `packaging/windows/test-installer-audio.ps1` as described in the packaging guide. Inspect the actual tutorial screenshots.
4. Commit the source, banner and tutorial screenshots, then push the commit. Create a tag/release for that exact commit.
5. Attach `build/installer/sattrak-1.0.0-windows-x64-setup.exe` and its SHA-256 checksum file to the release. Keep experimental builds marked **Pre-release**.
6. Update the README and tutorial download links whenever the release tag or asset filename changes. For a future stable release, GitHub also supports `/releases/latest/download/<asset-name>`; that link does not select development pre-releases.

The current preview's direct installer link is:

`https://github.com/ExplodingWater/sattrak/releases/download/v1.0.0-preview.20261103/sattrak-1.0.0-windows-x64-setup.exe`

The installer has no code-signing certificate yet. Validation on the development Windows machine is not a clean-machine installation/upgrade certification. Future releases should include those checks before being designated stable.
