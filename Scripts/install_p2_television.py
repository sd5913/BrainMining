"""Author P1's shared television escape in P2 without redressing the scene."""
import unreal,json,shutil,datetime
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/IntroP2Office'
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not E.get_game_world()
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
path='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2'
if not L.get_current_level().get_outer().get_path_name().startswith(path):
 assert L.load_level(path)
backup=root/'Saved/Backups'/('P2Television_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(root/'Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap',backup/'Lvl_EchoMazeToCards_P2.umap')
actors=A.get_all_level_actors()
stages=[a for a in actors if isinstance(a,unreal.CardRoomStage)]
entrances=[a for a in actors if isinstance(a,unreal.MiningGiantRoomEntrance)]
escapes=[a for a in actors if isinstance(a,unreal.MiningRoomEscape)]
assert len(stages)==len(entrances)==1
assert len(escapes)<=1
with unreal.ScopedEditorTransaction('Add P1 television model and escape to P2'):
 e=escapes[0] if escapes else A.spawn_actor_from_class(unreal.MiningRoomEscape,unreal.Vector(0,0,0))
 e.set_actor_label('P1 Television / Flood + Dig Escape')
 e.set_folder_path('Gameplay/TelevisionEscape')
 e.set_editor_property('card_stage',stages[0])
 e.set_editor_property('entrance',entrances[0])
 e.set_editor_property('preview_television',True)
parts=e.get_components_by_class(unreal.ActorComponent)
preview=[c for c in parts if 'TelevisionEditorPreview' in [str(t) for t in c.component_tags]]
assert len(preview)>40,len(preview)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
report=dict(actor=e.get_path_name(),preview_parts=len(preview),stage=stages[0].get_path_name(),entrance=entrances[0].get_path_name(),backup=str(backup))
(out/'tv_installed.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('P2_TELEVISION_INSTALLED '+json.dumps(report))
