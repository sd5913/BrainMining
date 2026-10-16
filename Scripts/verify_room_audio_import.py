import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
entries = json.loads((root / 'SourceAssets/RoomAudio/manifest.json').read_text(encoding='utf-8'))
results = []
for entry in entries:
    path = '/Game/Audio/Rooms/' + entry['asset']
    sound = unreal.load_asset(path)
    assert isinstance(sound, unreal.SoundWave), path
    assert sound.get_editor_property('num_channels') == 1, path
    assert sound.get_editor_property('looping') == entry['loop'], path
    assert sound.get_editor_property('sound_class_object').get_name() == 'SC_' + entry['category'], path
    assert sound.get_editor_property('override_concurrency'), path
    concurrency = sound.get_editor_property('concurrency_overrides')
    assert concurrency.get_editor_property('max_count') == (2 if entry['loop'] else 3), path
    duration = sound.get_editor_property('duration')
    assert abs(duration - entry['seconds']) < .01, path
    if entry['loop']:
        assert sound.get_editor_property('virtualization_mode') == unreal.VirtualizationMode.PLAY_WHEN_SILENT, path
    results.append(dict(asset=path, duration=duration, loop=entry['loop'], category=entry['category']))
(root / 'Saved/RoomAudioEngineValidation.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
unreal.log('ROOM_AUDIO_ENGINE_VALIDATION_OK: ' + str(len(results)))
