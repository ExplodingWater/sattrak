# Landsat 8 model credits

- Model: Landsat 8
- Creator: NASA / Christopher R. Meaney
- Source page: https://science.nasa.gov/3d-resources/landsat-8/
- Original GLB: https://assets.science.nasa.gov/content/dam/science/cds/3d/resources/model/landsat-8/Landsat%208.glb
- NASA collection: https://science.nasa.gov/3d-resources/
- Usage guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/

NASA lists its 3D resources as free to download and use, subject to its media
usage guidelines. Credit NASA and the named creator; no NASA endorsement is implied.

`source.glb` is the unmodified downloaded asset. `landsat8.mesh` is derived by
`scripts/bake_landsat8.py`: Draco decompression, application of the source node
scale, centering at the deployed bounding-box center, and packing positions,
normals, indices and source material colors/roughness. No textures are present
in the source model. Source dimensions and SHA-256 are in `conversion.json`.
The runtime uses approximate lighting rather than a full glTF PBR implementation.

The model retains the source asset's scale in 1:1 mode. Selecting it changes
visual geometry, not spacecraft mass, inertia, sensors or controller parameters.
