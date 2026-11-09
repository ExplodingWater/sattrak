# sattrak branding

The banner and installer artwork use real frames captured from sattrak's OpenGL renderer. No AI-generated imagery is used. Editing is limited to cropping, resizing, darkening behind text and adding the wordmark. The Hubble, Landsat 8 and ISS meshes are the same ones rendered in the application; their existing asset attribution still applies.

## Assets

- `docs/images/sattrak-banner.png`: repository hero, 1920 × 640, based on the Hubble capture.
- `docs/images/sattrak-earth.png`, `sattrak-landsat.png`, `sattrak-hubble.png`, `sattrak-iss.png`: unretouched application captures with interface overlays omitted by capture mode.
- `assets/branding/wizard-side.bmp`: 328 × 628 wizard welcome/finish artwork, based on the same Hubble shot.
- `assets/branding/wizard-small.bmp`: 110 × 110 Earth crop used on the remaining wizard pages.
- `assets/branding/sattrak.ico`: application and installer icon, derived from the captured Earth view.

The identity uses a lowercase **sattrak** wordmark, the description **Explore Earth from orbit**, a dark background and white type. There are no decorative accent bars on the banner or wizard artwork. The wordmark uses **Smash Hit thin**, supplied by the user in `smash_hit.zip`; supporting text uses Segoe UI. Image typography is baked in; no font installation is required by the app or installer. The font files themselves are not distributed in the repository or installer.

## Capture real scenes

From the executable directory, run `ag.exe --capture-branding`. It renders five deterministic, paused camera/model views and writes `capture-3.ppm`, `capture-7.ppm`, `capture-11.ppm`, `capture-15.ppm` and `capture-19.ppm`. Frames 3/7/11/15 supply the Earth/Landsat/Hubble/ISS images. The mode hides the ImGui overlay during framebuffer export and exits automatically. The resulting images remain genuine application renders.

The branded Windows wizard is built from `packaging/windows/sattrak.iss`; see [the installer guide](INSTALLER_PLAN.md)
