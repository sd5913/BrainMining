"""Import the supplied TV escape celebration cue without changing scene assets."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
task = unreal.AssetImportTask()
task.filename = str(root / 'SourceAssets/AudioFeedback/quotvictoriousquot-sound-effect.mp3')
task.destination_path = '/Game/Audio/Rooms'
task.destination_name = 'S_TVEscapeVictory'
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset(task.destination_path + '/' + task.destination_name)
assert isinstance(sound, unreal.SoundWave) and task.imported_object_paths
sound.set_editor_property('looping', False)
sound.set_editor_property('sound_class_object', unreal.load_asset('/Game/Audio/Mix/SC_Effects'))
sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
assert unreal.EditorAssetLibrary.save_loaded_asset(sound, False)
music = unreal.load_asset('/Game/Audio/Pilot/M_DreamyFlashback_Loop')
assert music.get_editor_property('looping')
assert music.get_editor_property('virtualization_mode') == unreal.VirtualizationMode.PLAY_WHEN_SILENT
report = {'cue': sound.get_path_name(), 'duration': sound.get_editor_property('duration'),
          'exploration_loop_keeps_silent_timeline': True}
out = root / 'Saved/Validation/HUDMusic'
out.mkdir(parents=True, exist_ok=True)
(out / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
