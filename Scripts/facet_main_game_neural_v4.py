"""Retain the validated neural sculptures while faceting the surrounding cave."""
import bpy,bmesh,runpy,json,shutil,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];src=ROOT/'Art/MainGame/RoutesV3/TunnelNeuralV4';out=ROOT/'Art/MainGame/RoutesV4/TunnelNeuralV4';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(next(src.glob('*.blend'))))
meshes=[o for o in bpy.data.objects if o.type=='MESH'];print('OBJECTS '+str([(o.name,len(o.data.polygons)) for o in meshes]),flush=True)
tunnel=next(o for o in meshes if 'Cutter' in o.name)
validate=runpy.run_path(str(ROOT/'Art/Mining/TunnelCutter/validate_tunnel.py'),run_name='helpers')
specs=json.loads((src/'neural_rocks.json').read_text());group=tunnel.vertex_groups.new(name='Broad_Cave_Walls')
for v in tunnel.data.vertices:
 protected=abs(v.co.x)>52 or any((v.co-Vector(s['probe_m'])).length<6 for s in specs)
 if not protected:group.add([v.index],1.0,'REPLACE')
bpy.ops.object.select_all(action='DESELECT');tunnel.hide_set(False);tunnel.select_set(True);bpy.context.view_layer.objects.active=tunnel
original=tunnel.data.copy();accepted=False
for ratio in [.40,.55,.7]:
 tunnel.data=original.copy()
 dec=tunnel.modifiers.new('Faceted_Broad_Walls','DECIMATE');dec.ratio=ratio;dec.vertex_group=group.name;dec.vertex_group_factor=1;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
 bm=bmesh.new();bm.from_mesh(tunnel.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(tunnel.data);bm.free()
 top=validate['topology'](tunnel.data);top.update(validate['intersections'](tunnel.data));print('FACET_TOPOLOGY '+json.dumps(top),flush=True)
 if top['connected_components']==1 and top['signed_volume_m3']>0 and all(top[k]==0 for k in ['boundary_edges','nonmanifold_edges','degenerate_faces','duplicate_faces','self_intersection_pairs']):accepted=True;break
assert accepted,top
for poly in tunnel.data.polygons:poly.use_smooth=False
def export(objects,path):
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.hide_set(False);o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False)
export([tunnel],out/'Tunnel_Cutter.fbx')
oldplan=json.loads((src/'placement_plan.json').read_text());active=[oldplan['objects'][i] for i in range(0,16,2)]
blocks=[]
for spec in active:
 ob=bpy.data.objects.get(spec['name']);assert ob,spec['name'];blocks.append(ob)
export(blocks,out/'Tunnel_Blockers.fbx')
for name in ['centerlines.json','neural_rocks.json','plan_summary.json']:shutil.copy2(src/name,out/name)
summary=json.loads((out/'plan_summary.json').read_text());summary.update(surface_style='angular-lowpoly',geometry_retained=False);(out/'plan_summary.json').write_text(json.dumps(summary,indent=2))
oldplan['objects']=active;(out/'placement_plan.json').write_text(json.dumps(oldplan,indent=2))
(out/'tunnel_validation.json').write_text(json.dumps(dict(top,passed=True),indent=2));(out/'progress.json').write_text(json.dumps(dict(passed=True,phase='complete',surface_style='angular-lowpoly',ratio=ratio),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Neural_Faceted_Validated.blend'))
print('NEURAL_FACETS_COMPLETE',flush=True)
