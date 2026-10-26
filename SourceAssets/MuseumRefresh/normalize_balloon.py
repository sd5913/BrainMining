"""Normalize the source FBX's inconsistent units before Unreal import."""
import bpy,pathlib
root=pathlib.Path(__file__).resolve().parent
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(root/'balloon/source/Balloon.fbx'))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
assert len(objects)==1
obj=objects[0];bpy.context.view_layer.objects.active=obj
obj.select_set(True);obj.dimensions=(.85,.85,1.275)
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
bpy.ops.export_scene.fbx(filepath=str(root/'balloon/source/Balloon_UE.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_ALL',axis_forward='-Y',axis_up='Z',path_mode='STRIP',bake_anim=False)
