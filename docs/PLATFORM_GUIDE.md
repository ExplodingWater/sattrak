# Run sattrak on macOS or Debian/Kali

Before native `.dmg` and `.deb` packages exist, build the C++ application on the target system. The Windows `ag.exe` and Windows setup program are not native Mac/Linux builds. Python is not required to build or run the GUI.

Requirements: CMake 3.20 or newer, a C++20 compiler, Git, and a graphical desktop with OpenGL 3.3 support. Build in a directory your account owns. Do not run sattrak with `sudo`.

## macOS

Install Apple's Command Line Tools, then CMake using [Homebrew](https://brew.sh/) if you use it:

```sh
xcode-select --install
brew install cmake
```

Wait for the Command Line Tools installation to finish before continuing. A current Xcode/Apple Clang toolchain is recommended. Run these commands in a native Terminal session on the Mac you want to use; the default build targets that machine's architecture (Apple Silicon or Intel).

```sh
git clone --recurse-submodules https://github.com/ExplodingWater/sattrak.git
cd sattrak
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cd build
./ag
```

This produces a command-line-launched native executable, not a Finder `.app` bundle yet. The source already requests a forward-compatible OpenGL core context on macOS and links Apple's audio frameworks. A universal app bundle, signing/notarization and a `.dmg` remain packaging work; native desktop behavior still needs validation on real Macs.

## Debian, Kali, Ubuntu and related distributions

Use a maintained release with a C++20-capable compiler (for example GCC 12 or newer). Install build tools and OpenGL/X11 development dependencies:

```sh
sudo apt update
sudo apt install build-essential cmake git pkg-config xorg-dev libgl1-mesa-dev libasound2-dev libcurl4-openssl-dev
git clone --recurse-submodules https://github.com/ExplodingWater/sattrak.git
cd sattrak
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cd build
./ag
```

This deliberately uses the X11 backend, matching the current Linux GUI CI configuration. On a Wayland desktop it needs XWayland; install `xwayland` if your desktop does not already provide it. Run from a graphical desktop terminal, not a bare TTY or an SSH session without a working display. Native Wayland support is not the initial supported route because the current display controls rely on desktop window positioning and monitor behavior.

In a VM, enable the guest's supported 3D acceleration and graphics drivers. If OpenGL initialization fails, `sudo apt install mesa-utils` followed by `glxinfo -B` helps inspect the available renderer and OpenGL version. A successful build alone does not prove that the VM provides a usable OpenGL 3.3 context.

## Assets, preferences and existing clones

CMake copies `assets/`, `earth_texture.jpg`, `LICENSE` and `TERMS.md` beside the executable. Keep them together. Launching from `build` as above puts the existing relative preferences and recordings in that writable directory. Do not copy only `ag` to `/usr/bin`; a future `.deb` needs proper read-only asset paths and per-user writable state.

For an existing clone, run `git submodule update --init --recursive` before configuring. Use separate build directories for each OS/architecture; never reuse a Windows CMake cache on macOS/Linux. A failed parallel build caused by low memory can be retried with `cmake --build build --parallel 2`.

## Current platform differences

- Bundled spacecraft, offline satellite data, UI and simulation are the intended cross-platform path. Choose offline data on first launch.
- App update checks use GitHub on every launch, with WinHTTP on Windows and libcurl on macOS/Linux
- Online catalog refresh currently uses Windows WinHTTP. macOS/Linux report that online updates are unavailable and fall back to the bundled/saved catalog.
- Native orbit-file Browse dialogs are currently Windows-only. On macOS/Linux, type a file path into **Primary file** or **Comparison file**, then click Load. Recording **Save As** has a text path field on these systems.
- Restart the guided tour from **Settings → General → Restart guided tutorial**. Its first-run preference is per user; most older preferences and recording paths are still relative to the launch folder.
- **Settings → Video → Interface → Control and window outlines** toggles outlines independently of **High contrast**. Disabling outlines preserves contrast colors, section dividers and keyboard focus indicators.

The repository has Windows, Linux and macOS compilation/test jobs. Interactive behavior, audio devices, high-DPI screens, fullscreen transitions and clean installations still need checks on each actual target OS before native packages are advertised as validated.

## Optional graphical checks

Run these from the build directory in a working graphical desktop session:

```sh
./ag --tutorial-smoke-test
./ag --orbit-smoke-test
./ag --display-smoke-test
```

The display check temporarily switches window/fullscreen modes and restores them. These commands also write screenshot captures beside the executable. CTest alone does not exercise the actual display/GPU/audio environment.

Dependency references: [GLFW build dependencies and backend options](https://www.glfw.org/docs/latest/compile.html), [GLFW platform compatibility](https://www.glfw.org/docs/latest/compat.html), [Apple Command Line Tools](https://developer.apple.com/documentation/xcode/installing-the-command-line-tools), [Homebrew CMake](https://formulae.brew.sh/formula/cmake).
