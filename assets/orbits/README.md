# Recorded orbit replay

Open **Recorded orbit replay / import** in Flight Director & Camera. Load a file, seek or play it, and optionally load a second trajectory for a gold comparison path and separation at the same UTC. Export writes station range, elevation, visibility above the configured mask, and comparison separation to a new CSV. Existing files are not overwritten.

The supplied demo files are synthetic circular trajectories, not satellite observations. Regenerate them with `python scripts/make_demo_orbits.py`.

CSV requires a frame declaration and this exact header. Positions are kilometers and velocities kilometers per second:

```csv
# frame=EME2000
utc,x_km,y_km,z_km,vx_km_s,vy_km_s,vz_km_s
2026-09-13T00:00:00Z,7000,0,0,0,7.5,0
2026-09-13T00:01:00Z,6985,450,0,-0.48,7.48,0
```

Single-segment CCSDS OEM text (KVN) is also supported. Use Earth-centered UTC samples in EME2000/J2000 or ECEF/ITRF. TEME, XML, other time systems, and multiple segments are rejected. Samples must be strictly increasing; interpolation is cubic Hermite with no extrapolation. Limits: 64 MiB and 500,000 samples.

CSV data rows must have exactly seven comma-separated, nonempty fields. Use decimal points, finite numeric positions/velocities, valid UTC dates, and the specified kilometer units. Extra/missing commas, NaN/Infinity, trailing junk, duplicate/reversed times and invalid Earth-centered states are rejected with a line-specific error in the replay window. A failed load preserves the existing primary and comparison trajectories. UTF-8 filenames, an optional UTF-8 BOM, LF/CRLF line endings, surrounding field whitespace and fractional UTC seconds are supported. Quoted fields and other CSV column layouts are not supported. Generic launch CSVs containing only time/altitude or latitude/longitude must be converted to this Cartesian state format first.

`Test_Orbit_Replay` verifies import, analytic interpolation, UTC/frame handling and malformed CSV rejection. `ag.exe --orbit-smoke-test` exercises primary/comparison loading, seeking and invalid-import preservation with the real OpenGL viewer.

Earth orientation uses approximate GMST, without precession, nutation, polar motion or measured Earth-orientation parameters. Station visibility uses a spherical Earth and geometric elevation. Replay positions follow the imported ephemeris; attitude remains a separate simulation. This is an exploration and comparison tool, not operational navigation software.
