import bpy,json
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
out=root/'Saved/Validation/CardRobot';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'SourceAssets/CardRobot/character.fbx'))
report=[]
for obj in bpy.data.objects:
    item=dict(name=obj.name,type=obj.type,location=list(obj.location),rotation=list(obj.rotation_euler),scale=list(obj.scale),dimensions=list(obj.dimensions))
    if obj.type=='MESH':
        item.update(vertices=len(obj.data.vertices),materials=[m.name for m in obj.data.materials],bounds=[list(obj.matrix_world@Vector(c)) for c in obj.bound_box],groups=[g.name for g in obj.vertex_groups],shapes=list(obj.data.shape_keys.key_blocks.keys()) if obj.data.shape_keys else [])
    if obj.type=='ARMATURE':item['bones']=[dict(name=b.name,head=list(b.head_local),tail=list(b.tail_local)) for b in obj.data.bones]
    report.append(item)
(out/'fbx-inspect.json').write_text(json.dumps(dict(objects=report,actions=[a.name for a in bpy.data.actions],images=[dict(name=i.name,path=i.filepath) for i in bpy.data.images]),indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(out/'inspect.blend'))
