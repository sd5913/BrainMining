"""Remove the sealing remesh's tiny faces while protecting the cavern pillars."""
import bpy,bmesh,runpy,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'Art/MainGame/RoutesV4/TunnelCavernV2'
bpy.ops.wm.open_mainfile(filepath=str(out/'TunnelCavernV2_RoutesV2.blend'))
tunnel=next(o for o in bpy.data.objects if o.type=='MESH' and 'Cutter' in o.name)
validation=runpy.run_path(str(ROOT/'Art/Mining/TunnelCutter/validate_tunnel.py'),run_name='helpers')
specs=json.loads((out/'pillars.json').read_text());group=tunnel.vertex_groups.new(name='Broad_Cave_Walls')
for v in tunnel.data.vertices:
 protected=abs(v.co.x)>52 or any((Vector((v.co.x,v.co.y,0))-Vector((s['center_m'][0],s['center_m'][1],0))).length<2.5 for s in specs)
 if not protected:group.add([v.index],1.0,'REPLACE')
bpy.ops.object.select_all(action='DESELECT');tunnel.hide_set(False);tunnel.select_set(True);bpy.context.view_layer.objects.active=tunnel
original=tunnel.data.copy();accepted=False
for ratio in [.18,.3,.45,.6,.8]:
 tunnel.data=original.copy();dec=tunnel.modifiers.new('Broad_Facets_After_Seal','DECIMATE');dec.ratio=ratio;dec.vertex_group=group.name;dec.vertex_group_factor=1;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
 top=validation['topology'](tunnel.data);top.update(validation['intersections'](tunnel.data));print('CAVERN_FACETS '+json.dumps(top),flush=True)
 if top['connected_components']==1 and top['signed_volume_m3']>0 and all(top[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs']):accepted=True;break
assert accepted,top
for p in tunnel.data.polygons:p.use_smooth=False
bpy.ops.export_scene.fbx(filepath=str(out/'Tunnel_Cutter.fbx'),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False)
(out/'tunnel_validation.json').write_text(json.dumps(dict(top,passed=True),indent=2));(out/'facet_refinement.json').write_text(json.dumps(dict(passed=True,ratio=ratio,before_faces=len(original.polygons),after_faces=len(tunnel.data.polygons)),indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Cavern_Faceted_Validated.blend'))
