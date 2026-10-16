"""Independent source-solid clearance checks before Unreal import."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/MainGame/RoutesV5';report={'passed':False,'stages':[]}
for stage in ['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']:
 folder=ART/stage;bpy.ops.wm.open_mainfile(filepath=str(folder/(stage+'_RoutesV5.blend')))
 obj=bpy.data.objects['SM_RoutesV5_'+stage+'_Cutter'];obj.data.calc_loop_triangles()
 tree=BVHTree.FromPolygons([v.co[:] for v in obj.data.vertices],[tuple(t.vertices) for t in obj.data.loop_triangles],all_triangles=True)
 paths=json.loads((folder/'centerlines.json').read_text());bad=[];clearance=1000.;samples=0
 for path in paths:
  for k in range(9,len(path['points'])-9,3):
   p=Vector(path['points'][k])
   for z in [-.48,0,.48]:
    hit=tree.find_nearest(p+Vector((0,0,z)));clearance=min(clearance,hit[3]);samples+=1
    if hit[3]<.42:bad.append([path['name'],k,z,hit[3]])
 features=json.loads((folder/'features.json').read_text());platforms=[]
 for p in features['platforms']:
  top=Vector(p['top_m']);floor=tree.ray_cast(top+Vector((0,0,.05)),Vector((0,0,-1)),40);roof=tree.ray_cast(top+Vector((0,0,.05)),Vector((0,0,1)),40)
  headroom=[]
  for dx,dy in [(0,0),(.8,0),(-.8,0),(0,.8),(0,-.8)]:
   h=tree.ray_cast(top+Vector((dx,dy,.05)),Vector((0,0,1)),40);headroom.append(h[0].z-top.z if h[0] else 0)
  platforms.append(dict(name=p['name'],floor_m=list(floor[0]) if floor[0] else None,roof_m=list(roof[0]) if roof[0] else None,headroom=min(headroom),base=p['base_z_m']))
 record=dict(stage=stage,triangles=len(obj.data.polygons),samples=samples,min_clearance=clearance,obstructions=bad,platforms=platforms);report['stages'].append(record)
report['passed']=all(not r['obstructions'] and all(p['headroom']>2 and p['floor_m'] and p['floor_m'][2]>p['base'] for p in r['platforms']) for r in report['stages'])
(ROOT/'Saved/Validation/MainGameRoutesV5/source-clearance.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report),flush=True)
assert report['passed']
