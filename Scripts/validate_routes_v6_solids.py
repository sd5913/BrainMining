"""Measure residual solid intersections after the 0.01 mm vertex simplification."""
import sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Saved/Tools/geometry'))
import numpy as np,manifold3d as m
root=ROOT/'Art/MainGame/RoutesV6/LayerOwnership';cache={};records=[]
def load(name,result):
 key=(name,result)
 if key in cache:return cache[key]
 p=root/('Results' if result else '')/name;v=[];f=[]
 for l in p.read_text(encoding='utf-8').splitlines():
  a=l.split()
  if a[0]=='v':v.append(list(map(float,a[1:])))
  if a[0]=='f':f.append([int(a[k])-1 for k in [1,3,2]])
 s=m.Manifold(m.Mesh64(np.array(v),np.array(f,dtype=np.uint64)));assert s.status()==m.Error.NoError;cache[key]=s;return s
for op in json.loads((root/'operations.json').read_text(encoding='utf-8')):
 target=load(op['target'],True)
 for owner in op['owners']:
  tool=load(owner,(root/'Results'/owner).exists());overlap=(target^tool).volume();records.append(dict(target=op['target'],owner=owner,overlap_m3=overlap))
report=dict(passed=all(abs(r['overlap_m3'])<.001 for r in records),pairs=records,max_overlap_m3=max(abs(r['overlap_m3']) for r in records))
(ROOT/'Saved/Validation/MainGameRoutesV6/exact-ownership.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(len(records),'pairs',report['max_overlap_m3'],report['passed'])
