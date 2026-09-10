# Atmospheric entry preview

Enable **Atmospheric entry preview** in Camera & simulation settings. It starts at 80 km and follows a prescribed descent; set descent speed to zero to inspect an altitude. Disable to return to 650 km. Satellite speed is adjustable in 1:1 mode. This is not a drag-integrated orbit, breakup or survival simulation.

A coarse log-interpolated atmospheric density table covers 0–200 km. Above 200 km visible heating is suppressed. Air-relative velocity subtracts Earth atmospheric co-rotation and includes prescribed descent. Density does not depend on visual Earth rotation settings.

Windward triangle area is computed from the actual Landsat mesh at physical scale and current attitude. The low-poly model uses bus and panel surface grids. The sum does not resolve occlusion or self-shadowing and is an approximate effective area, not a silhouette measurement. Effective nose radius is estimated from that area; it is not local curvature. The Sutton–Graves Earth stagnation heating correlation (1.74153e-4 SI) gives a heat-flux estimate. It is not a validated detailed heat distribution over the satellite.

Glow particles originate on windward surfaces and advect into a disturbed downstream wake. Normal display scaling affects rendering only. The warning threshold of 100 kW/m² is a visualization threshold, not a certified structural limit or ionization threshold. Mute suppresses only the sound; the warning continues to pulse. Sound is generated locally as PCM on Windows, with no external audio asset. Other platforms currently show the visual warning only. Pause preserves the warning at the frozen state. Leaving the heating condition or switching to the CFD tab stops the alarm. Returning to an active heating condition resumes it unless muted.

References: [NASA Sutton–Graves application](https://ntrs.nasa.gov/api/citations/20060004824/downloads/20060004824.pdf), [NASA reentry and breakup](https://orbitaldebris.jsc.nasa.gov/reentry/).
