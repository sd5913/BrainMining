"""Import the supplied thought bubble without changing its pixels or alpha."""
import unreal,pathlib,shutil
def import_thought():
 root=pathlib.Path(unreal.Paths.project_dir())
 source=pathlib.Path(r'C:/Users/Lenovo/Documents/xwechat_files/wxid_ai786wh5k76j22_fe2b/temp/RWTemp/2026-10/9e20f478899dc29eb19741386f9343c8/278fe0ab010dd7ebc8417ff350f723c1.png')
 target=root/'Art/WenSi/UI/ThoughtBubbleRight.png';target.parent.mkdir(parents=True,exist_ok=True)
 if not target.exists():shutil.copy2(source,target)
 task=unreal.AssetImportTask();task.filename=str(target);task.destination_path='/Game/WenSi/UI';task.destination_name='T_ThoughtBubbleRight';task.automated=True;task.replace_existing=False;task.save=False
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=unreal.load_asset('/Game/WenSi/UI/T_ThoughtBubbleRight');assert texture
 texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('max_texture_size',2048)
 unreal.EditorAssetLibrary.save_loaded_asset(texture)
 print('WENSI_THOUGHT_IMPORTED',texture.get_path_name())
import_thought()
