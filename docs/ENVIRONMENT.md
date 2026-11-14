# Environment and targeting

sattrak starts in **1:1 mode**, with meter-scale spacecraft and one simulation second per real second

Use **Settings → General → Environment mode → Mini mode** for the previous enlarged-spacecraft overview and accelerated simulation

## Sun and atmosphere

The solar disc follows the simulation date, or the CSV timestamp during replay, using the [USNO approximate solar coordinates](https://aa.usno.navy.mil/faq/sun_approx)

Its apparent diameter varies with Earth–Sun distance around half a degree, with limb darkening and adjustable optical glare in Settings

The same solar direction drives Earth lighting, atmospheric scattering and the spacecraft eclipse check

The atmosphere integrates sunlight along view rays with blue Rayleigh scattering and a forward-scattering haze contribution

This is an approximate visual environment, not a precision solar ephemeris or calibrated radiometric camera — it does not simulate solar flares, clouds, lunar eclipses or instrument exposure, and spacecraft shadowing remains a binary cylindrical Earth shadow

## Targeting

The selected ground latitude and longitude rotate with Earth into the scene's inertial frame

The attitude controller slews the body +Z boresight toward that location using reaction-wheel dynamics and the configured rate and acceleration limits

The procedural telescope has an aperture 1.15 m along body +Z in 1:1 mode — its camera, pointing error and Mini guide ray use the transformed aperture and boresight

The imported NASA mesh assets have no verified payload origin, instrument axis or gimbal metadata, so they use a clearly labelled simulated +Z sensor at the model origin — switching the visual model does not establish real mission-specific pointing capability

The virtual sensor camera excludes its own spacecraft housing and follows attitude while slewing instead of looking directly at the requested target

Lock requires both line of sight and less than 0.5 degrees of boresight error, and the Mini guide only turns green when both conditions hold

Spacecraft rendering now uses the same body-to-inertial rotation as the sensor calculations

## Verification

`Test_Solar_Environment` checks a reference solar position, seasonal direction and apparent diameter bounds

GUI smoke checks verify 1:1 startup, Mini transitions and agreement between the rendered body axis and telemetry across several attitudes

`ag.exe --environment-smoke-test` captures the Sun, Earth occultation, the illuminated limb and settings using the actual OpenGL renderer
