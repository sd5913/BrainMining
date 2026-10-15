"""Reuse the proven closed-solid Blender pipeline for three distinct route styles."""
import runpy,sys,shutil,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Scripts'));import main_game_routes_v3_plan as planner
source=(ROOT/'Scripts/build_main_game_routes_v2.py').read_text(encoding='utf-8')
source=source.replace("OUTROOT=BASE/'RoutesV2'","OUTROOT=BASE/'RoutesV3'")
source=source.replace('while len(bounds)<12:',"if len(bounds)>4:bounds=sorted(sorted(bounds,key=lambda ab:ab[1]-ab[0],reverse=True)[:4])\n    while len(bounds)<4:")
source=source.replace('assert len(bounds)==12,bounds','assert len(bounds)==4,bounds')
source=source.replace("factor=.88;blocks=ns['generate'](records,factor)","for r in records:\n        mid=(r['allowed_start_m']+r['allowed_end_m'])/2;half=(2.8 if r['path_index']==0 else 2.2)/(.88*2);r['allowed_start_m']=mid-half;r['allowed_end_m']=mid+half\n    factor=.88;blocks=ns['generate'](records,factor)")
source=source.replace("clean(tunnel);top=validation['topology'](tunnel.data)","clean(tunnel);before_faces=len(tunnel.data.polygons);keep_largest(tunnel);clean(tunnel);assert before_faces-len(tunnel.data.polygons)<=32,'Detached route segment';seal_final(tunnel);top=validation['topology'](tunnel.data)")
source=source.replace('==17','==9').replace('logical_air_pockets=17','logical_air_pockets=9').replace('len(trees)-17','len(trees)-9').replace("replace('count=20','count=16')","replace('count=20','count=8')")
source=source[:source.index("if __name__=='__main__':")]
scope={'__file__':str(ROOT/'Scripts/build_main_game_routes_v2.py'),'__name__':'regional_route_helpers'};exec(compile(source,'regional_route_helpers','exec'),scope);scope['plan']=planner.plan
base_clean=scope['clean'];bmesh=scope['bmesh']
def clean_v3(obj):
 base_clean(obj);bm=bmesh.new();bm.from_mesh(obj.data)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=3e-5);bm.verts.index_update();seen=set();duplicates=[]
 for f in bm.faces:
  key=tuple(sorted(v.index for v in f.verts))
  if key in seen:duplicates.append(f)
  seen.add(key)
 if duplicates:bmesh.ops.delete(bm,geom=duplicates,context='FACES_ONLY')
 bmesh.ops.dissolve_degenerate(bm,dist=4e-5,edges=list(bm.edges));bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
scope['clean']=clean_v3
def seal_final(obj):
 top=scope['validation']['topology'](obj.data);top.update(scope['validation']['intersections'](obj.data))
 keys=['boundary_edges','nonmanifold_edges','duplicate_faces','degenerate_faces','self_intersection_pairs']
 if not any(top[k] for k in keys):return
 bpy=scope['bpy'];bpy.context.view_layer.objects.active=obj
 for voxel in [.2,.24,.28]:
  mod=obj.modifiers.new('Seal_Landscape_Boolean_Seams','REMESH');mod.mode='VOXEL';mod.voxel_size=voxel;bpy.ops.object.modifier_apply(modifier=mod.name)
  mod=obj.modifiers.new('Smooth_Microscopic_Folds','SMOOTH');mod.factor=.35;mod.iterations=2;bpy.ops.object.modifier_apply(modifier=mod.name)
  mod=obj.modifiers.new('Retain_Faceted_Landscape','DECIMATE');mod.ratio=min(1,45000/len(obj.data.polygons));mod.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=mod.name);base_clean(obj)
  top=scope['validation']['topology'](obj.data);top.update(scope['validation']['intersections'](obj.data));print('LANDSCAPE_REPAIR '+str(voxel)+' '+str({k:top[k] for k in keys}),flush=True)
  if not any(top[k] for k in keys):return
 raise AssertionError(top)
scope['seal_final']=seal_final
original_pockets=scope['verify_air_pockets']
def verify_pockets_v3(obj,paths,records):
 try:return original_pockets(obj,paths,records)
 except Exception:
  print('RESIDUAL_TOPOLOGY '+str(scope['validation']['topology'](obj.data)),flush=True)
  scope['bpy'].ops.wm.save_as_mainfile(filepath=str(ROOT/'Saved/Validation/MainGameRoutesV3/seal_debug.blend'))
  raise
scope['verify_air_pockets']=verify_pockets_v3
wanted=next((a.split('=',1)[1] for a in sys.argv if a.startswith('--stage=')),None)
for i,stage in enumerate(planner.STAGES):
 if wanted and wanted!=stage:continue
 if i==3:
  out=ROOT/'Art/MainGame/RoutesV3'/stage;out.mkdir(parents=True,exist_ok=True)
  for f in (ROOT/'Art/MainGame/RoutesV2'/stage).iterdir():
   if f.is_file() and f.suffix in ['.json','.fbx','.blend']:shutil.copy2(f,out/f.name)
  summary=planner.plan(stage,i)[1];(out/'plan_summary.json').write_text(json.dumps(summary,indent=2));print('MIXED_REGION_GEOMETRY_RETAINED',flush=True)
 else:scope['build'](stage,i)
