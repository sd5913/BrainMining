import bpy,bmesh,json,pathlib,math
from mathutils import Vector,Matrix
root=pathlib.Path('H:/PolyU/Spotlight/BrainMining/SourceAssets/MuseumRefresh')
def clear():
 bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
def export(objects,path):
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,bake_anim=False,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True)
clear();bpy.ops.import_scene.gltf(filepath=str(root/'screen/screen.glb'));bpy.context.scene.frame_set(0)
# A shallow fold gives the screen depth without changing the blocking envelope.
for o in bpy.context.scene.objects:
 if o.name=='panel-left':o.rotation_euler.z=math.radians(-5)
 if o.name=='panel-right':o.rotation_euler.z=math.radians(5)
bpy.context.view_layer.update()
screen=[o for o in bpy.context.scene.objects if o.type=='MESH']
materials={m.name:list(m.diffuse_color) for o in screen for m in o.data.materials}
export(screen,root/'screen/Screen_UE.fbx')
clear();bpy.ops.import_scene.gltf(filepath=str(root/'balance/balance.glb'));bpy.context.scene.frame_set(0)
for o in list(bpy.context.scene.objects):
 if o.type!='MESH':continue
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 mw=o.matrix_world.copy();o.parent=None;o.matrix_world=mw
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.00001);bpy.ops.mesh.separate(type='LOOSE');bpy.ops.object.mode_set(mode='OBJECT')
parts=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 vs=[o.matrix_world@Vector(v) for v in o.bound_box];lo=Vector(tuple(min(v[i] for v in vs) for i in range(3)));hi=Vector(tuple(max(v[i] for v in vs) for i in range(3)))
 parts.append({'name':o.name,'min':list(lo),'max':list(hi),'vertices':len(o.data.vertices)})
(root/'balance/loose_parts.json').write_text(json.dumps(parts,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(root/'balance/Balance_parts.blend'))
(root/'screen/materials.json').write_text(json.dumps(materials,indent=2))

