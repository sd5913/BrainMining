"""Give the museum's smoke and glass alpha-shaped custom-depth silhouettes."""
import unreal,json,pathlib

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
lib=unreal.EditorAssetLibrary
dest='/Game/WenSi/Museum/Refresh/Materials/'
def copy(src,name):
    path=dest+name
    return unreal.load_asset(path) if lib.does_asset_exist(path) else lib.duplicate_asset(src,path)
master=copy('/Game/NiagaraExamples/Materials/MasterMaterials/M_SmokeAndFire_Sprites','M_MuseumSmokeSilhouette')
master.modify()
master.set_editor_property('allow_translucent_custom_depth_writes',True)
# Follow the smoke density instead of outlining the rectangular sprite cards.
master.set_editor_property('opacity_mask_clip_value',.12)
unreal.MaterialEditingLibrary.recompile_material(master)
instance=copy('/Game/NiagaraExamples/Materials/MI_SmokePuffLight_8x8','MI_MuseumSmokeSilhouette')
unreal.MaterialEditingLibrary.set_material_instance_parent(instance,master)
unreal.MaterialEditingLibrary.update_material_instance(instance)
glass=unreal.load_asset('/Game/WenSi/Museum/Materials/M_DisplayGlass')
assert isinstance(glass,unreal.Material)
glass.modify()
glass.set_editor_property('allow_translucent_custom_depth_writes',True)
glass.set_editor_property('opacity_mask_clip_value',.05)
unreal.MaterialEditingLibrary.recompile_material(glass)
for a in (master,instance,glass):assert lib.save_loaded_asset(a,False)
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumOutline/Unified/materials.json'
out.write_text(json.dumps({'smoke_parent':master.get_path_name(),'smoke_instance':instance.get_path_name(),'custom_depth':master.get_editor_property('allow_translucent_custom_depth_writes'),'alpha_cutoff':master.get_editor_property('opacity_mask_clip_value'),'glass_custom_depth':glass.get_editor_property('allow_translucent_custom_depth_writes')},indent=2))
print('MUSEUM_SMOKE_MATERIALS_READY')
