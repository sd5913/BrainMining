import unreal,json
from pathlib import Path
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem); w=e.get_game_world() or e.get_editor_world()
out={}
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
  if a.get_class().get_name() in ['MiningRoomEscape','CardRoomStage','MiningEncounterPlayerController']:
    d={'class':a.get_class().get_name(),'label':a.get_actor_label()}
    for p in ['phase','elapsed','water_z','b_wall_broken','b_entered_computer','b_exit_guidance','b_computer_checkpoint','computer_elapsed','cleared_nodes','attempts','failure_reason','run','room_id','played_cards','hand','opponent_health','player_health']:
      try:
        v=a.get_editor_property(p); d[p]=str(v)
      except Exception as ex: d[p]='<missing>'
    out[a.get_name()]=d
Path(unreal.Paths.project_dir()+'Saved/Validation/P2Repair/probe.json').write_text(json.dumps(out,indent=2),encoding='utf8')
