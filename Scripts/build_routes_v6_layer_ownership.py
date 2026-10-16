"""Partition closed layer solids directly in double precision with manifold3d."""
import sys,json,hashlib,traceback,math
from pathlib import Path
from collections import Counter
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Saved/Tools/geometry'))
import numpy as np
import manifold3d as m
ART=ROOT/'Art/MainGame/RoutesV6/LayerOwnership';OUT=ART/'Results';OUT.mkdir(exist_ok=True)
operations=json.loads((ART/'operations.json').read_text(encoding='utf-8'));solids={};report={'passed':False,'results':[],'hashes':{}}
def load(name):
    if name in solids:return solids[name]
    data=(ART/name).read_bytes();report['hashes'][name]=hashlib.sha256(data).hexdigest();verts=[];faces=[]
    for line in data.decode('utf-8').splitlines():
        p=line.split()
        if p[0]=='v':verts.append(tuple(map(float,p[1:])))
        elif p[0]=='f':faces.append(tuple(int(p[k])-1 for k in [1,3,2]))
    solid=m.Manifold(m.Mesh64(np.array(verts,dtype=np.float64),np.array(faces,dtype=np.uint64)))
    assert solid.status()==m.Error.NoError,(name,solid.status())
    assert solid.volume()>0,(name,solid.volume())
    solids[name]=solid;return solid
try:
    for operation in operations:
        name=operation['target'];solid=load(name)
        for owner in operation['owners']:solid=solid-load(owner)
        assert solid.status()==m.Error.NoError and not solid.is_empty(),(name,solid.status())
        solid=solid.simplify(.00001)
        mesh=solid.to_mesh64();points=mesh.vert_properties;triangles=mesh.tri_verts
        edges=Counter(tuple(sorted((int(t[k]),int(t[(k+1)%3])))) for t in triangles for k in range(3))
        assert all(count==2 for count in edges.values()),name
        solids[name]=solid
        lines=['# Exclusive layer solid, clockwise world metres']
        lines+=['v %.12f %.12f %.12f'%tuple(p[:3]) for p in points]
        lines+=['f %d %d %d'%(int(t[0])+1,int(t[2])+1,int(t[1])+1) for t in triangles]
        (OUT/name).write_text('\n'.join(lines),encoding='utf-8')
        if len(triangles)>12000:
            box=solid.bounding_box();low=np.array(box[:3]);high=np.array(box[3:]);counts=np.ceil((high-low)/18).astype(int);chunks=[];volume=0
            folder=OUT/(Path(name).stem+'_chunks');folder.mkdir(exist_ok=True)
            for x in range(counts[0]):
                for y in range(counts[1]):
                    for z in range(counts[2]):
                        index=np.array([x,y,z]);a=low+(high-low)*index/counts;b=low+(high-low)*(index+1)/counts
                        cutter=m.Manifold.cube(tuple(b-a)).translate(tuple(a));piece=solid^cutter
                        assert piece.status()==m.Error.NoError
                        if piece.is_empty():continue
                        pm=piece.to_mesh64();pv=pm.vert_properties;pf=pm.tri_verts
                        file=folder/('%d_%d_%d.obj'%(x,y,z));text=['v %.12f %.12f %.12f'%tuple(v[:3]) for v in pv]+['f %d %d %d'%(int(t[0])+1,int(t[2])+1,int(t[1])+1) for t in pf]
                        file.write_text('\n'.join(text),encoding='utf-8');chunks.append(str(file.relative_to(OUT)));volume+=piece.volume()
            assert abs(volume-solid.volume())<.001,(name,volume,solid.volume())
            (OUT/(name+'.chunks.json')).write_text(json.dumps({'chunks':chunks,'length_cm':1800,'volume_m3':volume}),encoding='utf-8')
        report['results'].append(dict(target=name,triangles=len(triangles),volume=solid.volume(),nonmanifold=0));print(report['results'][-1],flush=True)
    report['passed']=True
except Exception:report['error']=traceback.format_exc();print(report['error'],flush=True)
(ART/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
