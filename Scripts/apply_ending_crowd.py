"""Bind acquired City Sample meshes and in-place walk clips to the ending only."""
import unreal
import shutil
from pathlib import Path
from datetime import datetime

ROOT = '/Game/EndingEmergence/Crowd'
MAP = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
assets = unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(ROOT)

def load(path):
    asset = unreal.load_asset(path)
    assert asset, path
    return asset

def animation(path, name):
    destination = ROOT + '/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(destination):
        assert unreal.EditorAssetLibrary.duplicate_asset(path, destination)
    result = load(destination)
    unreal.AnimationLibrary.set_is_root_motion_lock_forced(result, True)
    unreal.AnimationLibrary.set_root_motion_enabled(result, False)
    unreal.AnimationLibrary.set_root_motion_lock_type(result, unreal.RootMotionRootLock.REF_POSE)
    assert unreal.EditorAssetLibrary.save_loaded_asset(result)
    return result

male_walk = animation('/Game/CitySampleCrowd/Character/Anims/Loco/MTN_N_Walk_F', 'EE_CityMaleWalk')
female_walk = animation('/Game/CitySampleCrowd/Character/Anims/Loco/FTN_Set/FTN_N_Walk_F', 'EE_CityFemaleWalk')
male_idle = animation('/Game/CitySampleCrowd/Character/Anims/Loco/MTN_N_Idle', 'EE_CityMaleIdle')
female_idle = animation('/Game/CitySampleCrowd/Character/Anims/Loco/FTN_Set/FTN_N_Idle_Base', 'EE_CityFemaleIdle')
people = []
for gender, prefix, top, pants, walk, speed in [
    ('Male', 'm', 'crewneck', 'jeans', male_walk, 125),
    ('Female', 'f', 'scoopneck', 'jeans', load(ROOT+'/EE_Anim_FemaleWalk'), 105),
    ('Female', 'f', 'turtleneck_blazer', 'slacks', female_walk, 115),
    ('Male', 'm', 'buttonDown_blazer', 'slacks', load(ROOT+'/EE_Anim_OldManWalk'), 85),
]:
    mesh = f'/Game/CitySampleCrowd/Character/{gender}/NormalWeight/Meshes/{prefix}_tal_nrw_'
    person = unreal.EndingPedestrian()
    person.set_editor_property('driver', load(mesh+'base'))
    person.set_editor_property('clothing', [load(mesh+p) for p in ['body', top, pants, 'oxfords']])
    face = f'/Game/CitySampleCrowd/Character/{gender}/{prefix}_001'
    person.set_editor_property('face', load(face+f'/Face/{prefix}_001_nrw_FaceMesh'))
    hair = 'AfroFade' if prefix == 'm' else 'Coil'
    person.set_editor_property('hair', load(face+f'/Hair/Helmets/Hair_S_{hair}_Helmet_LOD5'))
    person.set_editor_property('walk', walk)
    person.set_editor_property('idle', male_idle if prefix == 'm' else female_idle)
    person.set_editor_property('speed', speed)
    people.append(person)

path = ROOT+'/DA_EndingCrowd'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    data = load(path)
else:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.EndingCrowdAssets)
    data = assets.create_asset('DA_EndingCrowd', ROOT, unreal.EndingCrowdAssets, factory)
assert data
data.set_editor_property('people', people)
assert unreal.EditorAssetLibrary.save_loaded_asset(data)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.EndingEmergence))
source = Path(unreal.Paths.project_content_dir(), 'EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap')
backup = Path(unreal.Paths.project_saved_dir(), 'Backups', 'EndingCrowd-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
backup.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(source, backup)
demo.set_editor_property('crowd_assets', data)
assert levels.save_current_level()
unreal.log('ENDING_CROWD_APPLIED: 4 outfits, 2 acquired packs, 240 walkers and central idle person; '+str(backup))
