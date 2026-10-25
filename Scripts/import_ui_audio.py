import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = root/'SourceAssets/UIAudio'
manifest = json.loads((source/'manifest.json').read_text(encoding='utf8'))
effects = unreal.load_asset('/Game/Audio/Mix/SC_Effects')
assert effects
for entry in manifest:
    task = unreal.AssetImportTask()
    task.filename = str(source/(entry['asset']+'.wav'))
    task.destination_path = '/Game/Audio/Pilot'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset('/Game/Audio/Pilot/'+entry['asset'])
    assert isinstance(sound, unreal.SoundWave)
    sound.set_editor_property('sound_class_object', effects)
    sound.set_editor_property('volume', entry['volume'])
    sound.set_editor_property('looping', False)
    sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    concurrency = unreal.SoundConcurrencySettings()
    concurrency.set_editor_property('max_count', 2)
    concurrency.set_editor_property('retrigger_time', .04)
    concurrency.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
    sound.set_editor_property('override_concurrency', True)
    sound.set_editor_property('concurrency_overrides', concurrency)
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
unreal.log('UI_AUDIO_IMPORT_OK')
