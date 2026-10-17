"""Position the real player for close TV review; use the game's own mining trace."""
import unreal,json,time,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/IntroP2Office'
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=E.get_game_world();assert w
escapes=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)
assert len(escapes)==1
e=escapes[0];pc=unreal.GameplayStatics.get_player_controller(w,0)
p=unreal.GameplayStatics.get_player_character(w,0);move=p.get_component_by_class(unreal.MiningCharacterMovementComponent)
mine=p.get_component_by_class(unreal.MiningComponent)
entry=e.get_editor_property('computer_entry')
e.call_method('OnRewardClaimed',args=(e.get_editor_property('card_stage').get_editor_property('room_id'),))
# Only this inspection session gets a longer timer so it can be photographed.
e.set_editor_property('flood_seconds',600)
e.set_editor_property('computer_seconds',600)
p.set_actor_location(entry+unreal.Vector(-2500,-600,200),False,True)
move.set_movement_mode(unreal.MovementMode.MOVE_NONE)
pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p.get_actor_eyes_view_point()[0],entry+unreal.Vector(0,100,150)))
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'start':time.monotonic(),'stage':'intact','shots_ready':[]}
def tick(dt):
 try:
  if not E.get_game_world():
   perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
   unreal.unregister_slate_post_tick_callback(handle);return
  elapsed=time.monotonic()-s['start']
  move.set_movement_mode(unreal.MovementMode.MOVE_NONE)
  if elapsed>60:
   perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
   unreal.unregister_slate_post_tick_callback(handle);return
  if elapsed>18 and s['stage']=='intact':
   p.set_actor_location(entry+unreal.Vector(-620,0,-75),False,True)
   pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p.get_actor_eyes_view_point()[0],entry+unreal.Vector(-308,0,0)))
   mine.select_pickaxe()
   s['stage']='mining';s['try_at']=0
  if s['stage']=='mining' and elapsed-s['try_at']>.4:
   s['try_at']=elapsed
   result=mine.try_mine()
   screen=e.get_editor_property('television_screen')
   cuts=screen.get_editor_property('successful_cuts')
   if cuts>0:
    s['stage']='dug';s['screen_cuts']=cuts
    s['last_mined_is_screen']=mine.get_editor_property('last_mined_rock')==screen
    s['single_escape']=len(escapes)==1
    s['sections']=len(e.get_editor_property('computer_rocks'))
    s['checkpoint']=e.get_editor_property('computer_checkpoint')
    p.set_actor_location(entry+unreal.Vector(-1650,-250,0),False,True)
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p.get_actor_eyes_view_point()[0],entry))
   (out/'tv_real_mining.json').write_text(json.dumps(s,indent=2),encoding='utf8')
 except Exception:
  s['error']=traceback.format_exc()
  (out/'tv_real_mining.json').write_text(json.dumps(s,indent=2),encoding='utf8')
  perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
  unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)

