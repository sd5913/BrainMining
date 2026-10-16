import json
import pathlib

import unreal

root = pathlib.Path(unreal.Paths.project_dir())
source = root / 'SourceAssets' / 'AudioPilot'
manifest = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
tools = unreal.AssetToolsHelpers.get_asset_tools()
classes = {}
for category in ('Music', 'Effects', 'Ambience', 'Voice'):
    asset_path = '/Game/Audio/Mix/SC_' + category
    sound_class = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else tools.create_asset('SC_' + category, '/Game/Audio/Mix', unreal.SoundClass, unreal.SoundClassFactory())
    classes[category] = sound_class
    unreal.EditorAssetLibrary.save_loaded_asset(sound_class)
mix_path = '/Game/Audio/Mix/SM_Player'
mix = unreal.load_asset(mix_path) if unreal.EditorAssetLibrary.does_asset_exist(mix_path) else tools.create_asset('SM_Player', '/Game/Audio/Mix', unreal.SoundMix, unreal.SoundMixFactory())
mix.set_editor_property('duration', -1.0)
unreal.EditorAssetLibrary.save_loaded_asset(mix)
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
    sound.set_editor_property('sound_class_object', classes['Music' if entry['asset'].startswith('M_') else 'Effects'])
    if entry['asset'] in ('S_BombFuse_Loop', 'S_BombExplosion'):
        concurrency = unreal.SoundConcurrencySettings()
        concurrency.set_editor_property('max_count', 4 if entry['loop'] else 2)
        concurrency.set_editor_property('resolution_rule', unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
        concurrency.set_editor_property('volume_scale', .6)
        sound.set_editor_property('override_concurrency', True)
        sound.set_editor_property('concurrency_overrides', concurrency)
    if entry['loop']:
        sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
        sound.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    if sound.get_editor_property('num_channels') != entry['channels']:
        raise RuntimeError('Channel count mismatch: ' + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Save failed: ' + path)
    results.append({'asset': path, 'channels': entry['channels'], 'loop': entry['loop']})
report = root / 'Saved' / 'AudioPilotImport.json'
report.write_text(json.dumps(results, indent=2), encoding='utf-8')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
routing = []
for asset_type in ('SoundWave', 'SoundCue'):
    for data in registry.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', asset_type), True):
        if not str(data.package_name).startswith('/Game/'):
            continue
        sound = data.get_asset()
        name = str(data.asset_name)
        category = 'Effects'
        if name.startswith('M_'):
            category = 'Music'
        elif name in ('S_EE_ClearMyHead', 'S_EE_BlockedThought'):
            category = 'Voice'
        elif name.startswith('A_') or name in ('S_EE_Cell', 'S_EE_Street', 'S_EE_Space'):
            category = 'Ambience'
        changed = False
        if sound.get_editor_property('sound_class_object') != classes[category]:
            sound.set_editor_property('sound_class_object', classes[category])
            changed = True
        if isinstance(sound, unreal.SoundWave) and (sound.get_editor_property('looping') or category == 'Voice'):
            if sound.get_editor_property('virtualization_mode') != unreal.VirtualizationMode.PLAY_WHEN_SILENT:
                sound.set_editor_property('virtualization_mode', unreal.VirtualizationMode.PLAY_WHEN_SILENT)
                changed = True
        if changed:
            if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
                raise RuntimeError('Routing save failed: ' + str(data.package_name))
        routing.append({'asset': str(data.package_name), 'category': category})
(root / 'Saved/AudioRouting.json').write_text(json.dumps(routing, indent=2), encoding='utf-8')
unreal.log('AUDIO_PILOT_IMPORT_OK: ' + str(len(results)))
