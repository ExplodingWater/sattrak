"""Bake the bundled NASA Landsat 8 GLB into the studio's static mesh format.
Build-only dependencies: numpy, DracoPy 2.0.0. No runtime decoder is needed.
The source asset has one mesh under a uniform-scale node, no textures/animation.
"""
import json,struct,sys,hashlib
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'build/model-tools'))
import DracoPy
root=Path(__file__).resolve().parents[1]/'assets/models/landsat8'
b=(root/'source.glb').read_bytes()
assert struct.unpack_from('<4sII',b)==(b'glTF',2,len(b))
n=struct.unpack_from('<I',b,12)[0];doc=json.loads(b[20:20+n]);blob=b[28+n:]
assert len(doc['meshes'])==1 and not doc.get('images') and not doc.get('animations')
scale=np.array(doc['nodes'][0]['scale']);assert np.allclose(scale,scale[0])
parts=[]
for primitive in doc['meshes'][0]['primitives']:
 assert primitive.get('mode',4)==4
 view=doc['bufferViews'][primitive['extensions']['KHR_draco_mesh_compression']['bufferView']]
 offset=view.get('byteOffset',0)
 mesh=DracoPy.decode(blob[offset:offset+view['byteLength']])
 pos=mesh.points*scale
 normals=mesh.normals
 assert normals.shape==pos.shape and np.isfinite(pos).all()
 material=doc['materials'][primitive['material']]['pbrMetallicRoughness']
 parts.append((pos,normals,mesh.faces,material))
positions=np.concatenate([p[0] for p in parts]);low=positions.min(axis=0);high=positions.max(axis=0)
center=(low+high)/2
radius=float(np.linalg.norm(positions-center,axis=1).max())
with (root/'landsat8.mesh').open('wb') as out:
 out.write(struct.pack('<8sIf',b'AEROMDL1',len(parts),radius))
 for pos,normals,faces,material in parts:
  vertices=np.zeros((len(pos),8),dtype='<f4');vertices[:,:3]=pos-center;vertices[:,3:6]=normals
  indices=np.asarray(faces,dtype='<u4').reshape(-1)
  assert indices.max()<len(vertices)
  out.write(struct.pack('<5fII',*material.get('baseColorFactor',[1,1,1,1]),material.get('roughnessFactor',1),len(vertices),len(indices)))
  out.write(vertices.tobytes());out.write(indices.tobytes())
summary={'source_sha256':hashlib.sha256(b).hexdigest(),'parts':len(parts),'vertices':len(positions),'triangles':sum(len(p[2]) for p in parts),'source_bounds_m':[low.tolist(),high.tolist()],'center_offset_m':center.tolist(),'radius_m':radius}
(root/'conversion.json').write_text(json.dumps(summary,indent=2)+'\n')
print(summary)
