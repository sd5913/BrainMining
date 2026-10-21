"""Import the user-supplied office click into the existing effects audio bus."""
import json
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
task=unreal.AssetImportTask()
task.filename=str(root/'SourceAssets/RoomAudio/Wav/S_OfficeMouseClick.wav')
task.destination_path='/Game/Audio/Rooms'
task.destination_name='S_OfficeMouseClick'
task.automated=True
task.replace_existing=True
task.save=False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound=unreal.load_asset('/Game/Audio/Rooms/S_OfficeMouseClick')
assert isinstance(sound,unreal.SoundWave) and task.imported_object_paths
sound.set_editor_property('looping',False)
sound.set_editor_property('volume',1.)
sound.set_editor_property('sound_class_object',unreal.load_asset('/Game/Audio/Mix/SC_Effects'))
sound.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
assert sound.get_editor_property('num_channels')==1
assert 0<sound.get_editor_property('duration')<2
assert unreal.EditorAssetLibrary.save_loaded_asset(sound,only_if_is_dirty=False)
report=root/'Saved/Validation/OfficeMouseClickImport.json'
report.parent.mkdir(parents=True,exist_ok=True)
report.write_text(json.dumps(dict(asset=sound.get_path_name(),seconds=sound.get_editor_property('duration'),passed=True),indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
