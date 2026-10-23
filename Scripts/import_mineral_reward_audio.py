import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
tasks=[]
for path in (root/'SourceAssets'/'Audio'/'MineralRewards').glob('*.wav'):
    task=unreal.AssetImportTask();task.filename=str(path);task.destination_path='/Game/Audio/Collectibles'
    task.destination_name=path.stem;task.automated=True;task.replace_existing=True;task.save=True;tasks.append(task)
assert len(tasks)==4
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    sound=unreal.load_asset(task.destination_path+'/'+task.destination_name)
    assert sound
    sound.set_editor_property('loading_behavior',unreal.SoundWaveLoadingBehavior.FORCE_INLINE)
    sound.set_editor_property('sound_class_object',unreal.load_asset('/Game/Audio/Mix/SC_Effects'))
    unreal.EditorAssetLibrary.save_loaded_asset(sound,False)
unreal.log('MINERAL_REWARD_AUDIO_IMPORTED')
unreal.SystemLibrary.quit_editor()
