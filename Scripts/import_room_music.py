import hashlib
import json
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir()).resolve()
source = root / 'SourceAssets/RoomMusic'
entries = json.loads((source / 'manifest.json').read_text(encoding='utf-8'))
music = unreal.load_asset('/Game/Audio/Mix/SC_Music')
if not music:
    raise RuntimeError('Music sound class is missing')
results = []
for entry in entries:
    filename = source / entry['file']
    if hashlib.sha256(filename.read_bytes()).hexdigest() != entry['sha256']:
        raise RuntimeError('Music hash mismatch: ' + entry['asset'])
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/Audio/RoomMusic'
    task.destination_name = entry['asset']
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(task.destination_path + '/' + task.destination_name)
    if not isinstance(sound, unreal.SoundWave) or not task.imported_object_paths:
        raise RuntimeError('Music import failed: ' + entry['asset'])
    sound.set_editor_property('looping', True)
    sound.set_editor_property('volume', 1.0)
    sound.set_editor_property('sound_class_object', music)
    sound.set_editor_property('loading_behavior', unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    if sound.get_editor_property('num_channels') != 2:
        raise RuntimeError('Music must be stereo: ' + entry['asset'])
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Music save failed: ' + entry['asset'])
    results.append(dict(asset=sound.get_path_name(), category='Music', loop=True, title=entry['title']))
(root / 'Saved/RoomMusicImport.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
unreal.log('ROOM_MUSIC_IMPORT_OK: ' + str(len(results)))
