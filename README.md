# sattrak

A C++20 aerospace simulation project with a spacecraft attitude estimator/controller,
a two-dimensional compressible Euler solver, and an interactive OpenGL studio.

- **mekf_adcs** runs a 60-second attitude-control simulation with an MEKF, gyro bias,
  vector sensors, rigid-body RK4 integration and reaction-wheel limits.
- **supersonic_cfd** solves a Mach 2.5 flow over a 15-degree compression ramp using
  finite volumes, Roe fluxes, MUSCL reconstruction and SSP-RK2.
- **ag** provides live 3D orbital/attitude visualization and a CFD explorer.

The numerical core uses the C++ standard library. The native GUI also uses vendored
GLFW, GLAD, Dear ImGui, ImPlot and stb, plus platform OpenGL libraries.

## Build and run

Requirements: CMake 3.20+, a C++20 compiler, and Python 3.8+ for optional reporting.
The GUI needs OpenGL 3.3. On Linux it also needs the development packages for the
chosen GLFW window system; an X11 build can use:

    sudo apt-get install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

Clone with dependencies:

    git clone --recurse-submodules https://github.com/ExplodingWater/sattrak.git

For an existing clone, run `git submodule update --init --recursive`.

From the repository directory:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=OFF
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure

For a headless build that does not configure any GUI dependencies:

    cmake -S . -B build-headless -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF
    cmake --build build-headless --config Release --parallel
    ctest --test-dir build-headless -C Release --output-on-failure

Set BUILD_TESTING=OFF to omit test targets. Optimization comes from the selected
CMake configuration, so Debug builds retain normal debugging behavior.

On Windows with Visual Studio:

    .uildReleasemekf_adcs.exe
    .uildReleasesupersonic_cfd.exe
    .uildReleaseag.exe

With a single-configuration generator on Linux/macOS:

    ./build/mekf_adcs
    ./build/supersonic_cfd
    ./build/ag

Launch the GUI from the repository root or its executable directory so it can find
earth_texture.jpg. The build copies that asset next to ag.

Earth uses NASA's 5400 × 2700 Blue Marble Next Generation surface imagery.
See [Earth texture credits](assets/earth/CREDITS.md) for its source and resolution.

The command-line programs write dated files beneath data/adcs and data/cfd in
the current directory, plus compatibility copies of telemetry_adcs.csv and
supersonic_wedge_shock.vtk. Open the VTK file in ParaView for geometry-aware contours.

## Tests and reproducible results

CTest runs three suites covering quaternion algebra, inertial momentum conservation,
MEKF covariance Cholesky factorization, bias estimation against RK4 truth, Roe fluxes,
limiters, independent constant-rate attitude propagation, wheel saturation,
invalid CFD states, uniform-flow preservation, the full wedge solve, and failure
to converge when the iteration budget or CFL is too small.

[Current reproduced benchmarks](docs/BENCHMARKS.md) are generated from freshly run
tests and simulations rather than manually entered results:

    python scripts/generate_benchmarks.py --bin-dir build/Release

Use --bin-dir build for a single-configuration generator. The script runs in an
isolated temporary directory, validates the exported CFD dataset, then writes
docs/BENCHMARKS.md only if every check succeeds.

To validate an existing default-case dataset:

    python scripts/plot_shock_validation.py supersonic_wedge_shock.vtk

Despite its historical name, this script reports actual VTK measurements without
generating a plot. It exits nonzero for missing/invalid data or failed scalar
benchmarks. It assumes the default flow conditions and samples i=nx-10, j=1.
Pressure, density and Mach tolerances are 1%, 3% and 3%, respectively.

## Numerical conventions and behavior

Quaternions are scalar-first Hamilton quaternions. rotate(v) maps an inertial
vector into body coordinates; rotateInverse(v) maps back. Body angular velocity
propagates by q_next = q * delta_q. The MEKF uses a body-frame error,
q_true = q_estimated * delta_q, and injects its correction on the right.

Wheel torque is bounded by both the torque limit and the remaining momentum
capacity over the integration step. The same applied torque drives spacecraft
acceleration and the opposing wheel-momentum change.

The CFD solver uses a global timestep constrained by the most restrictive cell.
It validates conservative states, halves a candidate timestep when an RK stage
loses positive density/internal energy, and reports failure if retries are exhausted.
The steady-state convergence criterion is a dimensionless residual across all four
conserved variables; reducing CFL alone cannot declare convergence. The CLI exits
nonzero for nonconvergence, numerical failure or export failure.

The CLI and native CFD dashboard share the attached weak-shock calculation.
Dashboard temperature comes from the numerical pressure and density. Numerical
shock angle is explicitly unmeasured. Scalar comparison labels describe the sampled
error, not validation of the entire field. Changing CFL updates the running solver.
Numerical failures pause the GUI solver with an error message.

Native ADCS physics advances in fixed 0.01-second steps. Time warp changes the number
of physics steps; orbital motion uses the same simulated time. Earth spin has an
explicit separate visualization multiplier, defaulting to 1.

## Scope and remaining work

The orbital view uses prescribed circular kinematics. Its controller currently uses
true attitude/rates and its MEKF receives ideal vector observations; the command-line
ADCS scenario exercises noisy sensors and estimator feedback. The CFD contour texture
is displayed in grid-index space; use VTK for physical mesh geometry.

These tests are regression checks on specified cases. Mesh refinement studies,
broader shock/expansion cases, statistical MEKF consistency tests, and interactive
GUI smoke tests remain useful next steps.

CI separates headless builds/tests from native GUI compilation on Windows, Linux
and macOS. A successful local Windows build does not establish that those remote
platform jobs have run.

## Layout

- include/common: math primitives, constants and integration utilities.
- include/adcs: attitude dynamics, estimator, controller, sensors and telemetry.
- include/cfd: grid, fluid states, fluxes, reconstruction, solver and exports.
- src: command-line programs and the native GUI.
- tests: numerical and end-to-end regression suites.
- scripts: dataset visualization and reproducible benchmark reporting.
- external: vendored GUI dependencies and their license files.

Project code is covered by [LICENSE](LICENSE); vendored dependencies retain their
respective licenses.


### Native studio controls and visuals

The four orbital panels follow the left and right window edges as the application
resizes. Drag a title bar to detach that panel; its custom placement survives
restarts. **Restore edge layout** in Flight Director reattaches all four panels.
Attachment preferences are stored in `panel_layout.txt` alongside ImGui's layout.
Hover telemetry rows, controls, chart areas or the ground track for two seconds
for an explanation. FPS and frame time are measured from real render timing.

Right-drag enters free flight from the current viewpoint. Mouse look has no yaw
or pitch limit; release right mouse or press Escape to release the cursor.
WASD moves, E/Space and Q/C move along the camera's up/down axis, and Shift boosts
speed. Earth now has a sun-dependent atmospheric scattering effect and a
procedural star background. This is an approximate visual atmosphere, not a
scientific radiative-transfer model.

Ground target cards use bundled location photographs. See
[photo credits](assets/stations/CREDITS.md) for authors, sources and licenses.
CMake copies the photographs beside the executable; normal use needs no network.

For a GPU rendering check, run `ag.exe --smoke-test` from a fresh scratch working
directory containing `earth_texture.jpg` and `assets/`. It opens a hidden OpenGL
window, checks camera rotation and all seven textures, verifies panel bounds at
1600x940, 900x1100 and 2400x800, and writes five PPM captures, including the 1:1 close-up and settings popup. It leaves the
normal ImGui settings untouched. The machine must support OpenGL 3.3.


Open **Settings / 1:1 mode** at the top of Flight Director to adjust look and wheel
sensitivity. In **1:1 physical scale & real time**, the model uses meter-scale
satellite dimensions relative to Earth and advances one simulated second per real
second. User speed is in km/s; Shift triples it. Satellite speed defaults to the
circular-orbit speed and can be varied independently (including zero) for
experimentation. Changes to satellite speed override circular orbital kinematics;
they are not a force-based orbital maneuver. A foreground dot and SAT-01 label
locate the satellite, including off-screen and Earth-occluded positions. The red
Arial readout beside the default chart column shows actual free-flight speed and
the selected speed. The mode switches immediately without a loading phase.

WASD over exposed 3D space enters free flight from any camera preset. Text entry,
active widgets and popups retain their keys. Scrolling over panels or popups never
zooms the scene. The hidden smoke checks cover those wheel-routing cases, preset
to free-flight movement, diagonal normalization and real-time/satellite-speed
independence in addition to the earlier rendering checks.


**Satellite Orbit (Centered + Left-Drag)** follows the moving satellite at screen
center. Left-drag rotates around it, including through the poles; the wheel
changes distance. Its angles and distance are separate from Earth orbit camera
settings. In 1:1 mode the default distance is 8 meters. Right-drag or WASD exits
into free flight. **Reset satellite orbit view** restores the default framing.


Settings includes **Satellite model** with **Low poly** and **Landsat 8**. The
NASA model is bundled offline (85,364 triangles and 33 material groups), loaded
once at startup, and can be switched immediately. Selection persists in
`satellite_model.txt`. Camera framing adjusts to the selected model; 1:1 mode
retains its source dimensions. Simulation mass/inertia and controller behavior
are unchanged. Model credits and conversion details are in
[assets/models/landsat8/CREDITS.md](assets/models/landsat8/CREDITS.md).

The baked mesh is checked in; normal builds require no model tools or downloads.
To regenerate it, install DracoPy 2.0.0 into `build/model-tools`, provide numpy,
and run `scripts/bake_landsat8.py` with Python. The original GLB is retained.

Landsat 8 uses a uniform `(0.3, 0.3, 0.3)` model scale in non-1:1 views.
Its physical-scale geometry in 1:1 mode is unchanged.

Settings > **Satellite model** also offers **Hubble Space Telescope**, **Terra**,
**GOES-R**, and the **International Space Station**, alongside Low poly and Landsat 8.
Selection is remembered locally. Models include NASA geometry, available base-color
textures, and automatic camera framing; large spacecraft are reduced in overview mode.
The 1:1 view preserves meter-scale geometry. Terra's inconsistent source units are
calibrated to its published 6.8 m bus length; its two missing upstream textures use
source material colors. Credits, source files, and conversion records are under
`assets/models/`. Rebuild the four new meshes with `python scripts/bake_spacecraft.py`
(requires NumPy and Pillow). Model choice changes geometry, including the entry-preview
surface estimate, but does not change mass, inertia, controllers, or orbital elements.

## Atmospheric entry preview

Camera & simulation settings includes an atmospheric-entry preview with altitude and descent controls. Windward spacecraft surfaces emit a glow and downstream wake based on approximate atmospheric heating. A flashing telemetry warning includes a mute/unmute button for the generated Windows alarm. See [model assumptions and limitations](docs/REENTRY.md).

The entry path is prescribed; this feature does not predict orbital decay, structural failure or spacecraft survival. NASA model assets and location photographs retain their own credits and usage terms in `assets/`; the project's MIT license does not override those terms.

## Satellite history and hidden research catalog

The Settings option **Satellite trail since t=0** starts off. History records even while hidden; the light-green line narrows at close range. Older samples are progressively simplified while preserving the starting point and discontinuities. History is for the current session only.

Enter **Up, Up, Down, Down, Left, Right, Left, Right, B, A, Enter** outside a text field to discover **Research satellites** in Settings. A red/yellow discovery caption appears for six real seconds, and the unlock is remembered locally. Enabling the mode switches to an Earth overview; cyan dots can be hovered for names and NORAD IDs.

The bundled catalog includes 282 unique objects from CelesTrak's science, Earth-resources and weather groups at the snapshot date. It is not an exhaustive list of every scientific mission. libsgp4 (Apache-2.0) propagates the actual element sets from the saved UTC epoch plus simulation time. ag.exe works offline. Data provenance and limitations are in [catalog credits](assets/catalog/CREDITS.md). To refresh: `python scripts/update_research_catalog.py`, then rebuild.

Earth always uses the highest-detail 256 x 384 sphere with 196,608 triangles. There is no distance-based mesh switching, and the lower-detail mesh is no longer allocated.

The top music bar plays local MP3, WAV, Ogg Vorbis and FLAC files from `assets/music` in filename order. Automatic playback starts after a randomized 45-60 second delay and fades in the first track. Previous/restart, pause/resume and next controls use Feather vector icons, with a seek bar showing elapsed and remaining time. Volume accepts both slider input and an exact integer from 0-200%, and is remembered locally. Audio files retain their own rights and are not covered by the source code's MIT license.

Every adjustable slider has an exact numeric entry box beside it. Typed values are clamped to the same safe range as the slider.

### Orbit replay, expert controls, and radio recordings

At first launch, choose optional internet updates or offline prediction. Settings can change this choice later. Online mode downloads and validates CelesTrak's science, resource and weather TLE catalogs in the background; failed requests retain the cached or bundled catalog. Checks are limited to once per two hours across restarts. Prediction begins at session UTC and advances with simulation time. This refreshes the research catalog, not the user-controlled spacecraft or imported replay. TLE format currently covers only supported five-digit NORAD IDs. Preferences and downloaded snapshots stay in ignored `local/` storage.

On Windows, Alt+F4 and the main close button ask for confirmation with a native Yes/No dialog. Active or stopped unsaved path recordings get a warning that exiting discards them. Cancelling resumes without integrating the time spent in the dialog. Music startup and between-song silence no longer display placeholder text.

Flight Director & Camera opens a recorded-orbit importer with play/pause, seeking, playback speed, trajectory history, station visibility, comparison against a second ephemeris, and CSV report export. [Supported formats and synthetic examples](assets/orbits/README.md) cover CSV and single-segment CCSDS OEM text.

The same window offers **Record path**, **Stop recording**, **Save**, **Save As...**, and **Open in replay**. Stop retains an unsaved draft. Save exports a uniquely named CSV beside its journal in `local/recordings` beneath the launch folder; Save As opens a native file dialog on Windows (an editable destination path on macOS/Linux). Canceling the dialog retains the draft. Starting another recording or exiting normally discards unsaved work; explicitly saved exports remain. Recording takes one Earth-fixed position/velocity sample per simulation second, with a UTC origin at recording start. Pausing simulation pauses sampling. Flushed journals allow recovery after an abnormal process interruption; recovery trims only an incomplete final line and preserves the original. At least two valid samples are required to save or replay, and existing output files are never overwritten. Large trajectory discontinuities stop recording so separate segments can be saved.

For imported or recorded orbits, **Calculate passes in this file** lists the selected station's acquisition/loss UTC, duration and maximum elevation above its configured mask. Clicking a row starts replay at acquisition. An asterisk marks coverage-clipped passes. Search is limited to seven days and uses 10-second brackets with refined crossing times; very short grazing passes can be missed. Calculations use a spherical Earth and approximate Earth rotation, without terrain, atmospheric refraction or measured Earth-orientation corrections. Analytic equatorial-pass tests verify crossing and peak calculations.

The release work remaining is tracked in [the v1.0 checklist](docs/V1_0.md).

Press **F1** inside the app to open the command console (on some Macs, Fn+F1). Initial commands are `help`, `clear`, `radio.status`, `radio.stop`, `radio.mute`, `radio.unmute`, and `radio.play 01 --volume 0.1 --endat 02:30`. Numbers select the matching filename prefix in `assets/radio` (01, 02, etc.); quoted full filenames without a directory are also accepted. Other paths and links outside this directory are rejected. Volume ranges from 0 to 2 and defaults to 0.1. End times accept seconds, MM:SS or HH:MM:SS; invalid, nonpositive or out-of-duration limits are ignored. Explicit playback works even if automatic radio is disabled, respects mute, fades in over five seconds, and starts a new automatic wait after stopping when automatic radio is enabled. This first console is in-app only; it does not read commands from the background terminal.

**Reset all expert settings to defaults** immediately restores every expert control, applies default spacecraft properties, and resets wheel momentum, the estimator and sensor drift. It disables numerical propagation, stopping an active numerical-orbit recording to keep trajectory segments separate. **Mute radio communications** silences radio audio without stopping its playback or schedule. Settings has exact numeric and slider controls for master and radio volume (0–200%). Master affects music, radio and the reentry alarm; the music bar controls music separately. Radio defaults to 10%, and a command's `--volume` overrides the radio level for that clip. Audio levels persist across launches.

The top button in Settings opens **About sattrak**, crediting Martin Hafizi | ExplodingWater, linking to his GitHub account, and displaying “Made with love.” Enabling research satellites preserves the current camera perspective and position.

Arrow keys look around over a 3D view, entering free flight as right-drag does. Look speed follows camera sensitivity and elapsed time; text fields, active controls and popups retain their keys. Reentry warning tones use miniaudio on Windows, macOS and Linux, with a separate output from music/radio and the existing warning mute button. Device initialization errors appear in telemetry. Close spacecraft rendering requests 24-bit depth and uses a camera-space bounding-sphere near plane to reduce z-fighting on layered geometry such as the ISS.

Settings includes expert controls for spacecraft mass and principal inertia, reaction-wheel torque and momentum, pointing slew limits, integration step, gravity-gradient torque, sensor noise and bias, and station elevation mask. Optional numerical orbit propagation uses RK4 with selectable J2 and atmospheric drag; drag has configurable area, coefficient and density multiplier. Density is an approximate altitude table, not a weather-driven atmosphere model. These controls are session settings and illustrative physical models, not validated mission parameters. Imported replay trajectories remain controlled by their files.

The Radio menu provides occasional recorded amateur contacts and Morse reception. Each event plays one clip, fading in over five seconds to the configured radio level, multiplied by master volume. The initial wait and the silence after each clip are randomized to 15–30 minutes; the menu can disable, stop or manually start them. `radio.stop` stops both automatically scheduled and command-started clips, then schedules a fresh wait. These are recordings, not a live receiver. The six clips use CC0; see [recording credits](assets/radio/CREDITS.md). ISS recordings are not included pending permission.

### Camera views and quiet playback

Multi View beside Camera Perspectives provides 1–8 independent numbered cameras, either bordered tiles or separate desktop windows. Turn it off to return to View 1. More views increase GPU load. Satellite Orbit can approach 0.25 meters from the spacecraft center in physical scale; entering model geometry is possible.

Physical-scale targeting uses a conservative acceleration-limited attitude slew (0.25 degrees/s; Hubble 0.1 degrees/s), with a yellow estimated countdown on the selected station. Settings offers a quick-reposition override. These are illustrative controller limits, not validated spacecraft parameters; the ETA is for pointing, not the next unobstructed ground pass. Existing angular momentum can take time to settle.
