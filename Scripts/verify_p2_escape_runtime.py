import unreal,time,json
from pathlib import Path
e=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);w=e.get_game_world()
out=Path(unreal.Paths.project_dir())/'Saved/Validation/P2Repair/escape_runtime.json'
state={'t':time.monotonic(),'step':0,'rows':[]}
def tick(dt):
 try:
  now=time.monotonic(); esc=next(iter(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)),None); stage=next(iter(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage)),None); pawn=unreal.GameplayStatics.get_player_pawn(w,0)
  if not esc or not stage or not pawn:return True
  if state['step']==0:
   unreal.SystemLibrary.execute_console_command(w,'CardRoom.P2AutoPlay');state['step']=1;state['t']=now;state['rows'].append({'event':'autoplay'})
  elif state['step']==1 and now-state['t']>25:
   state['rows'].append({'event':'after_cards','phase':str(esc.phase),'water_z':esc.water_z});state['step']=2;state['t']=now
  elif state['step']==2 and now-state['t']>3:
   state['rows'].append({'event':'flood','phase':str(esc.phase),'water_z':esc.water_z});pawn.set_actor_location(unreal.Vector(15000,3600,0),False,True);state['step']=3;state['t']=now
  elif state['step']==3 and now-state['t']>1:
   state['rows'].append({'event':'drown','phase':str(esc.phase),'water_z':esc.water_z});state['step']=4;state['t']=now
  elif state['step']==4 and now-state['t']>3:
   state['rows'].append({'event':'retry','phase':str(esc.phase),'pawn':str(pawn.get_actor_location()),'water_z':esc.water_z});out.write_text(json.dumps(state,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);return False
 except Exception as ex:
  out.write_text(json.dumps({'error':str(ex),'rows':state['rows']},indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);return False
 return True
handle=unreal.register_slate_post_tick_callback(tick)
