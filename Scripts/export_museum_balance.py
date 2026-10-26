import bpy,pathlib,json
from mathutils import Vector
root=pathlib.Path('H:/PolyU/Spotlight/BrainMining/SourceAssets/MuseumRefresh')
bpy.ops.wm.open_mainfile(filepath=str(root/'balance/Balance_parts.blend'))
def export_parts(names,name,pivot):
 bpy.ops.object.select_all(action='DESELECT');objs=[bpy.data.objects[n] for n in names]
 for o in objs:o.select_set(True)
 bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join();o=bpy.context.object
 for v in o.data.vertices:v.co-=Vector(pivot)
 o.location=(0,0,0);o.name=name
 bpy.ops.export_scene.fbx(filepath=str(root/'balance'/(name+'.fbx')),use_selection=True,object_types={'MESH'},apply_unit_scale=True,bake_anim=False,axis_forward='-Y',axis_up='Z')
export_parts(['pan/iron','pan/iron.001','pan/iron.002','pan/iron.004','pan/iron.011'],'BalanceBeam',(0,0,-.14))
export_parts(['pan/brass','pan/brass.001'],'BalancePan',(-.42,0,-.5575))
export_parts(['blacksmith-forge-and-armoury-hanging-scale_0','blacksmith-forge-and-armoury-hanging-scale_0.001'],'BalancePivot',(0,0,-.037))
# The borrowed model's rods cannot follow the game's independent pan motion.
# Existing telescoping suspension is retained; only beam, hinge and pans replace.
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.mesh.primitive_cube_add(size=1);o=bpy.context.object;o.name='ConductiveBlock'
bevel=o.modifiers.new('Soft machined edges','BEVEL');bevel.width=.035;bevel.segments=3
bpy.ops.object.modifier_apply(modifier=bevel.name)
bpy.ops.export_scene.fbx(filepath=str(root/'balance/ConductiveBlock.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,bake_anim=False,axis_forward='-Y',axis_up='Z')
