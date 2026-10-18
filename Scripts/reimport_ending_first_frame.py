"""Reimport only the ending movie's extracted first frame in the open editor."""
from pathlib import Path
import json
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
source = project / 'SourceAssets/EndingEmergence/Video/EndingFinaleFirstFrame.png'
assert source.is_file(), 'Extract the new movie first frame before importing.'
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/EndingEmergence/PaperFinale'
task.destination_name = 'T_EndingFinaleFirstFrame'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/EndingEmergence/PaperFinale/T_EndingFinaleFirstFrame')
assert texture and task.imported_object_paths, 'First frame import failed.'
texture.set_editor_property('never_stream', True)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
result = {'asset': texture.get_path_name(), 'source': str(source), 'width': texture.blueprint_get_size_x(), 'height': texture.blueprint_get_size_y()}
out = project / 'Saved/Validation/EndingNewFirstFrame'
out.mkdir(parents=True, exist_ok=True)
(out / 'import-result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
unreal.log('ENDING_FIRST_FRAME_REIMPORTED ' + json.dumps(result))
