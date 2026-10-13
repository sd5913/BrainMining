"""Separate the existing Fab toilet lid, preserving its UVs and geometry.

Run with Blender after the source mesh is exported into Saved/HeavenPreview.
The original Fab mesh stays untouched. UE applies its original material.
"""
import bpy,bmesh,json,pathlib
from mathutils import Vector
root=pathlib.Path(__file__).resolve().parents[1];saved=root/'Saved/HeavenPreview';out=root/'Art/HeavenPreview/Toilet';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(saved/'toilet_work.blend'))
record=next(r for r in json.loads((saved/'toilet_geometry.json').read_text(encoding='utf8')) if r['object']=='toilet1')
source=bpy.data.objects['toilet1'];groups=record['groups']
lid_groups=[i for i,g in enumerate(groups) if g['min'][2]>2.59 and g['max'][2]>3 and g['max'][1]<1.21 or i in (4,5)]
assert set(lid_groups)=={4,5,10,11,12,13,14,15,16,17},lid_groups
lid_indices={v for i in lid_groups for v in groups[i]['vertex_indices']};hinge=Vector((-.028739,.8,2.645))
report={'source':'/Game/Fab/Toilet/toilet1','hinge_blender_m':list(hinge),'hinge_ue_cm':[-2.8739,-80,264.5],'lid_groups':lid_groups,'parts':[]}
for name,keep_lid in [('Body',False),('Lid',True)]:
 obj=source.copy();obj.data=source.data.copy();obj.name='SM_HeavenToilet'+name;bpy.context.collection.objects.link(obj)
 obj.data.transform(source.matrix_world)
 bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.ensure_lookup_table()
 remove=[v for v in bm.verts if (v.index in lid_indices)!=keep_lid];bmesh.ops.delete(bm,geom=remove,context='VERTS');bm.to_mesh(obj.data);bm.free()
 # Export with an explicit hinge origin and meter units; UE converts to cm.
 if keep_lid:
  for vertex in obj.data.vertices:vertex.co-=hinge
 obj.location=(0,0,0);obj.rotation_euler=(0,0,0);obj.scale=(1,1,1)
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 path=out/(obj.name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=False,add_leaf_bones=False,bake_anim=False,path_mode='AUTO')
 report['parts'].append({'name':obj.name,'vertices':len(obj.data.vertices),'polygons':len(obj.data.polygons),'file':str(path.relative_to(root))})
(out/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_TOILET_PARTS',json.dumps(report))
