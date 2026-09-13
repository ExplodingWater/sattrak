"""Bake static NASA GLBs to AEROMDL2 with transformed geometry and base-color textures.
Build dependencies: numpy and Pillow. Terra needs a documented source-unit correction.
"""
import base64, hashlib, io, json, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def bake(name):
    folder = ROOT / 'assets/models' / name
    source = (folder / 'source.glb').read_bytes()
    assert struct.unpack_from('<4sII', source) == (b'glTF', 2, len(source))
    n = struct.unpack_from('<I', source, 12)[0]
    doc = json.loads(source[20:20+n]); blob = source[28+n:]
    assert not doc.get('animations') and not doc.get('skins')
    def accessor(index):
        a=doc['accessors'][index]; assert 'sparse' not in a
        v=doc['bufferViews'][a['bufferView']]
        dtype=np.dtype({5120:'i1',5121:'u1',5122:'<i2',5123:'<u2',5125:'<u4',5126:'<f4'}[a['componentType']])
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        data=np.ndarray((a['count'],width),dtype,blob,
            v.get('byteOffset',0)+a.get('byteOffset',0),
            strides=(v.get('byteStride',width*dtype.itemsize),dtype.itemsize)).copy()
        if a.get('normalized'):
            data=np.maximum(data.astype(float)/np.iinfo(dtype).max,-1)
        return data
    groups={}
    def visit(index,parent):
        node=doc['nodes'][index]
        if 'matrix' in node: local=np.array(node['matrix']).reshape(4,4,order='F')
        else:
            x,y,z,w=node.get('rotation',[0,0,0,1])
            local=np.eye(4)
            local[:3,:3]=np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]) @ np.diag(node.get('scale',[1,1,1]))
            local[:3,3]=node.get('translation',[0,0,0])
        world=parent@local
        if 'mesh' in node:
            for primitive in doc['meshes'][node['mesh']]['primitives']:
                assert primitive.get('mode',4)==4 and not primitive.get('extensions')
                attr=primitive['attributes']; pos=accessor(attr['POSITION']).astype(float)
                pos=pos@world[:3,:3].T+world[:3,3]
                faces=accessor(primitive['indices']).reshape(-1,3) if 'indices' in primitive else np.arange(len(pos)).reshape(-1,3)
                faces=faces.astype(np.int64)
                if np.linalg.det(world[:3,:3])<0: faces=faces[:,[0,2,1]]
                if 'NORMAL' in attr:
                    normals=accessor(attr['NORMAL'])@np.linalg.inv(world[:3,:3])
                else:
                    normals=np.zeros_like(pos); tri=pos[faces]
                    cross=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
                    for j in range(3): np.add.at(normals,faces[:,j],cross)
                normals/=np.maximum(np.linalg.norm(normals,axis=1)[:,None],1e-20)
                mat=primitive.get('material',-1)
                material=doc['materials'][mat] if mat>=0 else {}
                tex=material.get('pbrMetallicRoughness',{}).get('baseColorTexture',{})
                transform=tex.get('extensions',{}).get('KHR_texture_transform',{})
                uvkey='TEXCOORD_'+str(transform.get('texCoord',tex.get('texCoord',0)))
                uv=accessor(attr[uvkey]) if uvkey in attr else np.zeros((len(pos),2))
                uv=uv*np.array(transform.get('scale',[1,1])); angle=transform.get('rotation',0)
                uv=uv@np.array([[np.cos(angle),np.sin(angle)],[-np.sin(angle),np.cos(angle)]])+transform.get('offset',[0,0])
                groups.setdefault(mat,[]).append((pos,normals,uv,faces))
        for child in node.get('children',[]): visit(child,world)
    for node in doc['scenes'][doc.get('scene',0)]['nodes']: visit(node,np.eye(4))
    # Terra's GLB exporter lost its physical units. Its bus longitudinal extent
    # (source Y, transformed X) is 13621.760955 units; NASA lists a 6.8 m bus.
    unit_scale=6.8/13621.760955433994 if name=='terra' else 1.0
    for group in groups.values():
        for pos,_,_,_ in group: pos*=unit_scale
    points=np.concatenate([p[0] for group in groups.values() for p in group])
    low=points.min(axis=0); high=points.max(axis=0); center=(low+high)/2
    radius=float(np.linalg.norm(points-center,axis=1).max())+.001
    triangles=0
    with (folder/(name+'.mesh')).open('wb') as out:
        out.write(struct.pack('<8sIf',b'AEROMDL2',len(groups),radius))
        for mat,group in groups.items():
            material=doc['materials'][mat] if mat>=0 else {}
            pbr=material.get('pbrMetallicRoughness',{})
            factor=np.clip(pbr.get('baseColorFactor',[1,1,1,1]),0,1).tolist(); texture=''
            image_source=None
            if 'baseColorTexture' in pbr:
                tex=doc['textures'][pbr['baseColorTexture']['index']]
                img=doc['images'][tex['source']]
                if 'bufferView' in img:
                    view=doc['bufferViews'][img['bufferView']];start=view.get('byteOffset',0)
                    raw=blob[start:start+view['byteLength']]
                elif img['uri'].startswith('data:image/') and ';base64,' in img['uri']:
                    raw=base64.b64decode(img['uri'].split(',',1)[1])
                else:
                    raw=None
                    print(name,'missing upstream texture; using material color:',img['uri'])
                image_source=raw
            if image_source is not None:
                image=Image.open(io.BytesIO(image_source)).convert('RGBA')
                # Keep texture resolution, apply glTF's material factor once during baking.
                pixels=np.array(image).astype(float)/255
                pixels[:,:,:3]=np.power(pixels[:,:,:3],2.2)*np.array(factor[:3])
                pixels[:,:,:3]=np.power(pixels[:,:,:3],1/2.2); pixels[:,:,3]*=factor[3]
                texture=f'material-{mat}.png'
                Image.fromarray(np.uint8(np.clip(pixels*255,0,255))).save(folder/texture)
            vertices=[]; indices=[]; offset=0
            for pos,normals,uv,faces in group:
                vertices.append(np.concatenate((pos-center,normals,uv),axis=1))
                indices.append(faces+offset);offset+=len(pos)
            vertices=np.asarray(np.concatenate(vertices),dtype='<f4')
            indices=np.asarray(np.concatenate(indices),dtype='<u4').reshape(-1)
            assert np.isfinite(vertices).all() and indices.max()<len(vertices)
            triangles+=len(indices)//3
            out.write(struct.pack('<5fII',*factor,pbr.get('roughnessFactor',1),len(vertices),len(indices)))
            encoded=texture.encode('ascii');out.write(struct.pack('<I',len(encoded)));out.write(encoded)
            out.write(vertices.tobytes());out.write(indices.tobytes())
    summary={'source_sha256':hashlib.sha256(source).hexdigest(),'source_unit_to_m':unit_scale,'parts':len(groups),
        'triangles':triangles,'source_bounds_m':[low.tolist(),high.tolist()],
        'dimensions_m':(high-low).tolist(),'center_offset_m':center.tolist(),'radius_m':radius}
    (folder/'conversion.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(name,summary)

if __name__=='__main__':
    for name in sys.argv[1:] or ['hubble','terra','goes','iss']: bake(name)
