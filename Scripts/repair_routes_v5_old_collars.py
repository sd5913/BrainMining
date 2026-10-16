import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
removed=[]
for a in actors.get_all_level_actors():
 if a.get_actor_label().startswith('LINK_BM_Campaign_MainGame03_46250_Collar_'):
  removed.append(a.get_actor_label());assert actors.destroy_actor(a)
assert levels.save_current_level()
(ROOT/'Saved/Validation/MainGameRoutesV5/old-collars.json').write_text(json.dumps(dict(passed=True,removed=removed),indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'QUIT_EDITOR')
