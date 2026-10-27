"""Split the approved scale for its existing runtime motion, retaining source UVs."""
import bpy,pathlib,json,shutil
from mathutils import Vector
root=pathlib.Path('H:/PolyU/Spotlight/BrainMining/SourceAssets/MuseumRefresh')
balance=root/'medieval_balance'
parts_file=root.parents[1]/'Saved/Validation/MuseumTweaks/SourceStaging/Scales_parts.blend'
if parts_file.exists():bpy.ops.wm.open_mainfile(filepath=str(parts_file))
else:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.gltf(filepath=str(balance/'source/scales.glb'))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH');bpy.context.view_layer.objects.active=o;o.select_set(True)
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.remove_doubles(threshold=.001);bpy.ops.mesh.separate(type='LOOSE');bpy.ops.object.mode_set(mode='OBJECT')
(balance/'import').mkdir(exist_ok=True)
for name,dest in [('Image_0','Base.png'),('Image_1','ORM.png'),('Image_2','Normal.png')]:
 im=bpy.data.images.get(name)
 if im:
  im.scale(2048,2048);im.filepath_raw=str(balance/'import'/dest);im.file_format='PNG';im.save()
groups={'Stand':[],'Beam':[],'LeftPan':[],'RightPan':[]}
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 vs=[o.matrix_world@Vector(v) for v in o.bound_box]
 cx=(min(v.x for v in vs)+max(v.x for v in vs))/2
 top=max(v.z for v in vs)
 role='Beam' if o.name in ('scales.028','scales.029','scales.030','scales.031') else ('RightPan' if cx>20 else 'LeftPan') if abs(cx)>20 and top<77 else 'Stand'
 groups[role].append(o)
report={}
for role,objects in groups.items():
 bpy.ops.object.select_all(action='DESELECT')
 for o in objects:o.select_set(True)
 bpy.context.view_layer.objects.active=objects[0];bpy.ops.object.join();o=bpy.context.object
 for v in o.data.vertices:
  x,y,z=v.co
  if role=='Stand':v.co=Vector((x*2.8,(y+1.9)*2.8,20+(z-.1791549)*150/(84-.1791549)))/100
  elif role=='Beam':v.co=Vector((x*90/54.15,(y+1.9)*2.8,(z-84)*1.8))/100
  else:
   pivot=54.15 if role=='RightPan' else -54.1
   v.co=Vector(((x-pivot)*4.2,(y+1.9)*4.2,(z-34.46)*2.65))/100
 o.location=(0,0,0);o.name='MedievalScale'+role
 path=balance/(o.name+'.fbx')
 bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,bake_anim=False,axis_forward='-Y',axis_up='Z')
 report[role]={'path':str(path),'vertices':len(o.data.vertices)}
weight=root/'mining_weight';weight.mkdir(parents=True,exist_ok=True)
shutil.copy2('C:/Users/11959/Downloads/mining_weight.glb',weight/'mining_weight.glb')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(weight/'mining_weight.glb'))
objs=[o for o in bpy.context.scene.objects if o.type=='MESH']
for o in objs:
 world=o.matrix_world.copy();o.parent=None;o.matrix_world=world
bpy.ops.object.select_all(action='DESELECT')
for o in objs:o.select_set(True)
bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
lo=Vector(tuple(min(v.co[i] for v in o.data.vertices) for i in range(3)));hi=Vector(tuple(max(v.co[i] for v in o.data.vertices) for i in range(3)))
center=(lo+hi)/2;center.z=lo.z
ratio=min(.54/(hi.x-lo.x),.54/(hi.y-lo.y),.34/(hi.z-lo.z))
for v in o.data.vertices:v.co=(v.co-center)*ratio
o.location=(0,0,0);o.name='MuseumMiningWeight'
for im in bpy.data.images:
 if im.type=='IMAGE' and im.packed_file:
  im.filepath_raw=str(weight/(im.name+'.png'));im.file_format='PNG';im.save()
bpy.ops.export_scene.fbx(filepath=str(weight/'MuseumMiningWeight.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,bake_anim=False,axis_forward='-Y',axis_up='Z')
report['Weight']={'materials':[m.name for m in o.data.materials],'vertices':len(o.data.vertices)}
(balance/'export.json').write_text(json.dumps(report,indent=2))
print('SCALE_AND_WEIGHT_PREPARED',report)
