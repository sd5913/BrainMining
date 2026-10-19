import hashlib
import json
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceAssets/CollectionAudio'
entries = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
effects = unreal.load_asset('/Game/Audio/Mix/SC_Effects')
if not effects:
    raise RuntimeError('Effects sound class is missing')
results = []
for entry in entries:
    filename = source / entry['file']
    if hashlib.sha256(filename.read_bytes()).hexdigest() != entry['sha256']:
        raise RuntimeError('Collectible audio hash mismatch: ' + entry['asset'])
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/Audio/Collectibles'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(task.destination_path + '/' + task.destination_name)
    if not isinstance(sound, unreal.SoundWave) or not task.imported_object_paths:
        raise RuntimeError('Collectible audio import failed: ' + entry['asset'])
    sound.set_editor_property('looping', False)
    sound.set_editor_property('volume', 1.0)
    sound.set_editor_property('sound_class_object', effects)
    sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    if sound.get_editor_property('num_channels') != 1:
        raise RuntimeError('Collectible audio must be mono: ' + entry['asset'])
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Collectible audio save failed: ' + entry['asset'])
    results.append(dict(item_id=entry['item_id'], asset=sound.get_path_name(), category='Effects', loop=False))
(root / 'Saved/CollectionAudioImport.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
unreal.log('COLLECTION_AUDIO_IMPORT_OK: ' + str(len(results)))
