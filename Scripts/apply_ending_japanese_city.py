import unreal, shutil
from pathlib import Path
from datetime import datetime
MAP='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo1'
CONTENT=Path(unreal.Paths.project_content_dir())
backup=Path(unreal.Paths.project_saved_dir(),'Backups','EndingDemo1-JapaneseCity-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
backup.parent.mkdir(parents=True,exist_ok=True)
source=CONTENT/'EndingEmergence/Maps/Lvl_EndingEmergence_Demo1.umap'
if source.exists(): shutil.copy2(source,backup)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
building_paths=[
 '/Game/JapaneseCity/Meshes/Buildings/SM_jctBld12_6A_low',
 '/Game/JapaneseCity/Meshes/Buildings/SM_jctBld12_6B_low',
 '/Game/JapaneseCity/Meshes/Buildings/SM_jctBld12_6C_low',
 '/Game/JapaneseCity/Meshes/Buildings/SM_jctBld24_12A_low',
]
prop_paths=[
 '/Game/JapaneseCity/Meshes/Props/SM_jctStreetLightA',
 '/Game/JapaneseCity/Meshes/Props/SM_jctBenchA',
 '/Game/JapaneseCity/Meshes/Foliage/SM_jctTreeA',
 '/Game/JapaneseCity/Meshes/Props/SM_jctTrafficLightA',
 '/Game/JapaneseCity/Meshes/Props/SM_jctRoadConeA',
]
buildings=[unreal.load_asset(p) for p in building_paths]
props=[unreal.load_asset(p) for p in prop_paths]
assert all(buildings), [p for p,x in zip(building_paths,buildings) if not x]
assert all(props), [p for p,x in zip(prop_paths,props) if not x]
demo.set_editor_property('city_building_meshes',buildings)
demo.set_editor_property('city_prop_meshes',props)
demo.apply_city_buildings()
assert levels.save_current_level()
unreal.log('ENDING_JAPANESE_CITY_APPLIED: low-poly building set, instanced street props, performance-safe density; backup='+str(backup))

