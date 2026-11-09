# Catalog sky

Settings → Video → Night sky controls catalog stars, constellation lines,
constellation names, limiting magnitude (0–6), and displayed star brightness.
These preferences are saved under `local/sky_settings.txt`. Lines and names
start disabled; stars start enabled. All data is bundled for offline use.
The same sky is rendered into the main view and each Multi View texture.
Earth and spacecraft occlude stars and guides normally.

The catalog contains 5,044 real stars, with position and visual magnitude
from the Extended Hipparcos compilation, and guide figures for all 88
constellations. Serpens has two label regions. Colors are an approximate
mapping from B-V; brightness uses a compressed display response to make
faint stars visible. The limiting magnitude is a display choice, not an
atmospheric or instrument visibility prediction.

J2000 equatorial coordinates map to the simulation's inertial axes: right
ascension zero points along +X, the north celestial pole along +Y, and RA
90 degrees along -Z. Sky drawing uses camera orientation without translation;
stars stay effectively at infinity, including during close spacecraft views.
No random stars are added. Missing or malformed assets produce a Settings
message and a console diagnostic rather than an invented replacement sky.

Earth starts at approximate Greenwich mean sidereal angle for session UTC.
Normal Earth spin advances that orientation; replay uses its trajectory UTC.
**Align Earth to simulation UTC** restores the approximate orientation and
normal spin after manual Earth rotation or Spin Warp. A paused session does
not follow wall-clock time. This uses the project's existing approximate
GMST transform; the star catalog stays fixed at J2000. It does not apply
precession, nutation, proper motion, aberration, parallax, Earth-orientation
corrections, atmospheric refraction, daylight extinction or light pollution.
It is a catalog-based visualization rather than a precision planetarium.
The existing illustrative Sun direction is independent of this sky model.

See [asset provenance and licensing](../assets/sky/CREDITS.md).

Validation:

```sh
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
# Requires a graphical desktop / OpenGL context; not registered in CTest.
./build/test_sky_renderer assets/sky build/sky-preview.ppm
```
