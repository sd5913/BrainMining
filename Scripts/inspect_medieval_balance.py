"""Inspect the approved CC-BY balance for independent animated pieces."""
import bpy,json,pathlib
from mathutils import Vector
root=pathlib.Path('H:/PolyU/Spotlight/BrainMining/SourceAssets/MuseumRefresh/medieval_balance')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(root/'source/scales.glb'))
bpy.context.scene.frame_set(0)
r=[]
for o in bpy.context.scene.objects:
 if o.type!='MESH':continue
 vs=[o.matrix_world@Vector(v) for v in o.bound_box]
 r.append({'name':o.name,'vertices':len(o.data.vertices),'min':[min(v[i] for v in vs) for i in range(3)],'max':[max(v[i] for v in vs) for i in range(3)],'materials':[m.name for m in o.data.materials if m]})
(root/'structure.json').write_text(json.dumps(r,indent=2))
staging=root.parents[2]/'Saved/Validation/MuseumTweaks/SourceStaging';staging.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(staging/'Scales.blend'))
print('MEDIEVAL_BALANCE_STRUCTURE',json.dumps(r))
