import hashlib
import json
import pathlib

import unreal

root = pathlib.Path(unreal.Paths.project_dir())
source = root/'SourceAssets/MainGameAudio'
manifest = json.loads((source/'runtime_manifest.json').read_text(encoding='utf-8'))
classes = {category:unreal.load_asset('/Game/Audio/Mix/SC_'+category) for category in ['Music','Effects','Ambience']}
assert all(classes.values()), 'Import AudioPilot mix assets first'
results = []
for entry in manifest:
    filename = source/entry['file']
    assert hashlib.sha256(filename.read_bytes()).hexdigest() == entry['sha256']
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/Audio/MainGame'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(task.destination_path+'/'+task.destination_name)
    assert isinstance(sound,unreal.SoundWave) and task.imported_object_paths, entry['asset']
    sound.set_editor_property('looping',entry['loop'])
    sound.set_editor_property('volume',1.)
    sound.set_editor_property('sound_class_object',classes[entry['category']])
    if entry['loop']:
        sound.set_editor_property('virtualization_mode',unreal.VirtualizationMode.PLAY_WHEN_SILENT)
        sound.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    if entry['asset']=='S_BombExplosion':
        concurrency=unreal.SoundConcurrencySettings()
        concurrency.set_editor_property('max_count',2)
        concurrency.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
        concurrency.set_editor_property('volume_scale',.6)
        sound.set_editor_property('override_concurrency',True)
        sound.set_editor_property('concurrency_overrides',concurrency)
    assert sound.get_editor_property('num_channels')==entry['channels'],entry['asset']
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound,only_if_is_dirty=False)
    results.append(dict(asset=sound.get_path_name(),category=entry['category'],channels=entry['channels']))
(root/'Saved/MainGameAudioImport.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
unreal.log('MAIN_GAME_AUDIO_IMPORT_OK: '+str(len(results)))
