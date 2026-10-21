import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceAssets/RoomAudio'
entries = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
classes = {name: unreal.load_asset('/Game/Audio/Mix/SC_' + name) for name in ('Effects', 'Ambience')}
assert all(classes.values())
results = []
for entry in entries:
    filename = source / entry['file']
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == entry['sha256']
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/Audio/Rooms'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(task.destination_path + '/' + task.destination_name)
    assert isinstance(sound, unreal.SoundWave) and task.imported_object_paths
    sound.set_editor_property('looping', entry['loop'])
    sound.set_editor_property('volume', 1.)
    sound.set_editor_property('sound_class_object', classes[entry['category']])
    concurrency = unreal.SoundConcurrencySettings()
    concurrency.set_editor_property('max_count', entry.get('max_count', 3 if not entry['loop'] else 2))
    concurrency.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
    sound.set_editor_property('override_concurrency', True)
    sound.set_editor_property('concurrency_overrides', concurrency)
    if entry['loop']:
        sound.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
        sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    assert sound.get_editor_property('num_channels') == 1
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
    results.append(dict(asset=sound.get_path_name(), loop=entry['loop'], category=entry['category']))
(root / 'Saved/RoomAudioImport.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
unreal.log('ROOM_AUDIO_IMPORT_OK: ' + str(len(results)))
