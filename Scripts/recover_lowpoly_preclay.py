"""Restore our immediately preceding saved checkpoint after a reporting error."""
import unreal,json,shutil,time
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV4'
partial=json.loads((OUT/'Clay/bake.json').read_text(encoding='utf-8'));assert not partial['passed'] and "name 'joint_records'" in partial['error']
source=Path(partial['backup'])/'Lvl_MainGame.umap';target=ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap'
assert source.is_file() and source.is_relative_to(ROOT/'Saved/Backups') and source.stat().st_size>1000000
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
shutil.copy2(target,OUT/'clay_reporting_error.umap')
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
unreal.SystemLibrary.collect_garbage()
shutil.copy2(source,target)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
regions=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion)]
assert len(regions)==4 and all(not r.generated_rock.actor_has_tag('BM_MainGame_3DDirtBaked') for r in regions)
(OUT/'clay_recovery.json').write_text(json.dumps(dict(passed=True,checkpoint=str(source),error='Reporting variable repaired before rebaking'),indent=2));unreal.log('LOWPOLY_PRECLAY_RECOVERED')
