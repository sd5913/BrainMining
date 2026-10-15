import json
import pathlib

import unreal

root = pathlib.Path(unreal.Paths.project_dir())
source = root / 'SourceAssets' / 'AudioPilot'
manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
tasks = []
for entry in manifest:
    task = unreal.AssetImportTask()
    task.filename = str(source / 'Wav' / (entry['asset'] + '.wav'))
    task.destination_path = '/Game/Audio/Pilot'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
results = []
for entry, task in zip(manifest, tasks):
    path = '/Game/Audio/Pilot/' + entry['asset']
    sound = unreal.load_asset(path)
    if not isinstance(sound, unreal.SoundWave) or not task.imported_object_paths:
        raise RuntimeError('Import failed: ' + path)
    sound.set_editor_property('looping', entry['loop'])
    sound.set_editor_property('volume', 1.0)
    if entry['loop']:
        sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    if sound.get_editor_property('num_channels') != entry['channels']:
        raise RuntimeError('Channel count mismatch: ' + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Save failed: ' + path)
    results.append({'asset': path, 'channels': entry['channels'], 'loop': entry['loop']})
report = root / 'Saved' / 'AudioPilotImport.json'
report.write_text(json.dumps(results, indent=2), encoding='utf-8')
unreal.log('AUDIO_PILOT_IMPORT_OK: ' + str(len(results)))
