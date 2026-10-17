import unreal,json
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);w=E.get_game_world()
d={'world':str(w)}
if w:
 pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0);d.update(time=unreal.GameplayStatics.get_time_seconds(w),pc=str(pc),pawn=str(pawn),cursor=pc.get_editor_property('show_mouse_cursor'),move_ignored=pc.is_move_input_ignored(),view=str(pc.get_view_target()))
 d['openings']=[]
 for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening):
  d['openings'].append(dict(name=a.get_name(),started=a.get_editor_property('started'),finished=a.get_editor_property('finished'),tick=a.is_actor_tick_enabled(),title_visible=a.get_editor_property('world_title').is_visible(),loc=str(a.get_actor_location())))
 d['halls']=[dict(name=a.get_name(),ready=a.get_editor_property('ready')) for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)]
 unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename=E:/BrainMining_CodexBuild/Saved/Validation/IntroP2Office/opening_before.png -nosuffix')
Path(unreal.Paths.project_dir(),'Saved/Validation/IntroP2Office/opening_runtime.json').write_text(json.dumps(d,indent=2),encoding='utf8')
