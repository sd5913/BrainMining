"""Import the supplied ending film and its exact opening frame, then its portal."""
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir())
folder='/Game/EndingEmergence/PaperFinale'
assets=unreal.AssetToolsHelpers.get_asset_tools()
for filename,name in [('Content/Movies/EndingEmergence/EndingFinale.mp4','MS_EndingFinale'),('SourceAssets/EndingEmergence/Video/EndingFinaleFirstFrame.png','T_EndingFinaleFirstFrame')]:
 task=unreal.AssetImportTask()
 task.filename=str(project/filename);task.destination_path=folder;task.destination_name=name
 task.automated=True;task.replace_existing=True;task.save=True
 assets.import_asset_tasks([task])
 assert unreal.load_asset(folder+'/'+name),name
source=unreal.load_asset(folder+'/MS_EndingFinale')
assert isinstance(source,unreal.FileMediaSource)
source.set_file_path(str(project/'Content/Movies/EndingEmergence/EndingFinale.mp4'))
source.set_editor_property('precache_file',True)
assert unreal.EditorAssetLibrary.save_loaded_asset(source)
frame=unreal.load_asset(folder+'/T_EndingFinaleFirstFrame')
frame.set_editor_property('never_stream',True)
frame.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
frame.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
assert unreal.EditorAssetLibrary.save_loaded_asset(frame)
exec(compile((project/'Scripts/apply_ending_video_portal.py').read_text(encoding='utf-8-sig'),'video_portal','exec'))
unreal.log('ENDING_SUPPLIED_VIDEO_READY: 864x480, 24fps, original movie and exact opening frame')
