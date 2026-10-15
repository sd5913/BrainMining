"""Require all live checks, then leave the saved MainGame ready to play."""
import unreal,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV4'
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
reports={name:json.loads((OUT/path).read_text(encoding='utf-8')) for name,path in dict(authoring='authoring.json',finishing='finishing.json',saved='saved_state.json',routes='runtime.json',effects='FX/runtime.json',rooms='LinkedRooms/runtime.json').items()}
for name,r in reports.items():assert r['passed'],name
assert reports['effects']['completed']
before=json.loads((OUT/'before.json').read_text(encoding='utf-8'))
for source,digest in before['source_hashes'].items():assert hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest,source
old=json.loads((ROOT/'Saved/Validation/MainGameMiningFX/prebuild.json').read_text(encoding='utf-8'))
for path,digest in old['protected_hashes'].items():
 if path.endswith('.umap'):continue
 assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,path
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.set_current_level_by_name('Lvl_MainGame');assert levels.save_current_level()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages();assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
result=dict(passed=True,level='/Game/MainGame/Maps/Lvl_MainGame',backup=before['backup'],checks={name:len(r.get('checks',[])) for name,r in reports.items()},regions=reports['routes']['regions'],real_input_cases=len(reports['effects']['samples']),minerals=reports['saved']['minerals']['deposits'],source_maps_unchanged=True,player_blueprint_and_c4_unchanged=True,route_centerlines_and_room_interfaces_unchanged=True,lazy_runtime_chunks=unreal.SystemLibrary.get_console_variable_int_value('bm.Mining.LazyRuntimeChunks'))
(OUT/'acceptance.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.log('LOWPOLY_MAIN_GAME_COMPLETE '+json.dumps(result))
