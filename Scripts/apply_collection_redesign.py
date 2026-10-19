"""Explicitly import supplied sprites and apply the editable collection layout."""
import datetime
import json
from pathlib import Path
import shutil
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/CollectionRedesign';out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
backup=out/('before_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir()
shutil.copy2(root/'Content/CollectionPrototype/UI/WBP_CollectionInspect.uasset',backup/'WBP_CollectionInspect.uasset')
assets=unreal.AssetToolsHelpers.get_asset_tools();folder='/Game/CollectionPrototype/UI/Redesign'
for source in sorted((root/'SourceAssets/CollectionInspect/Cropped').glob('*.png')):
    task=unreal.AssetImportTask()
    task.filename=str(source);task.destination_path=folder;task.destination_name=source.stem
    task.automated=True;task.replace_existing=True;task.save=True
    assets.import_asset_tasks([task])
    texture=unreal.load_asset(folder+'/'+source.stem);assert texture
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream',True)
    if source.stem=='T_CollectionPreviewDefault':texture.set_editor_property('srgb',False)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

# SceneColorHDR stores inverse opacity. Leave the inspection backdrop visible
# through the capture's empty pixels so UMG can blur and dim the actual room.
lib=unreal.MaterialEditingLibrary
material=unreal.load_asset(folder+'/M_CollectionPreview')
if not material:material=assets.create_asset('M_CollectionPreview',folder,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(material)
material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
sample=lib.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D)
sample.set_editor_property('parameter_name','Preview')
sample.set_editor_property('texture',unreal.load_asset(folder+'/T_CollectionPreviewDefault'))
sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
invert=lib.create_material_expression(material,unreal.MaterialExpressionOneMinus)
assert lib.connect_material_expressions(sample,'A',invert,'')
assert lib.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(invert,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(material);unreal.EditorAssetLibrary.save_loaded_asset(material)
assert unreal.CollectionUMGEditorLibrary.apply_collection_inspect_redesign()
bp=unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([bp])
(out/'authoring.json').write_text(json.dumps({'asset':bp.get_path_name(),'backup':str(backup),'sprites':5,'main_game_map_saved':False},indent=2),encoding='utf-8')
unreal.log('COLLECTION_REDESIGN_SAVED')
