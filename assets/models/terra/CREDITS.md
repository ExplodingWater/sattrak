# Terra

Credit: NASA / Christian A. Lopez.

Source: https://science.nasa.gov/3d-resources/terra/

Original: https://assets.science.nasa.gov/content/dam/science/cds/3d/resources/model/terra/Terra.glb

FBX inspected for missing images: https://github.com/nasa/NASA-3D-Resources/tree/master/3D%20Models/Terra

Downloaded September 12, 2026. NASA media usage guidelines apply: https://www.nasa.gov/nasa-brand-center/images-and-media/
No NASA endorsement is implied. This asset is separate from the software's MIT license.

Converted with scripts/bake_spacecraft.py: static transforms, centering, material colors and roughness. The GLB references Side_Panels_TERRA.tga and solarpanels.tga, neither included in the published folder or embedded in its FBX. Those parts retain the source material colors.

The source GLB has inconsistent physical units. Its bus longitudinal extent is 13621.760955 source units; the uniform conversion factor is 6.8/13621.760955 meters per source unit, calibrated to NASA's published 6.8 m bus length: https://terra.nasa.gov/About
This is approximate visual scale, not engineering CAD. See conversion.json for bounds and source checksum. Selecting the model does not change simulation mass, inertia or orbit.
