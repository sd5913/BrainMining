"""Import the user-supplied museum thought-bubble artwork into the UI path."""
import pathlib
import unreal

root = pathlib.Path(unreal.Paths.project_dir())
source = root / "Art/WenSi/UI/ThoughtBubbleRight.png"
assert source.is_file(), source

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = "/Game/WenSi/UI"
task.destination_name = "T_ThoughtBubbleRight"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset("/Game/WenSi/UI/T_ThoughtBubbleRight")
assert texture, "T_ThoughtBubbleRight import failed"
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("max_texture_size", 2048)
unreal.EditorAssetLibrary.save_loaded_asset(texture)
unreal.log("WENSI_THOUGHT_ATTACHMENT_IMPORTED")
