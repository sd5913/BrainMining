"""Render the authored StartRoom source with the same opening anchors as MainGame."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/PlaytestPolish/Opening';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
s={'phase':0,'start':time.monotonic(),'next':0,'busy':False};report={'passed':False,'checks':[]}
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def check(ok,msg):
 assert ok,msg
 report['checks'].append(msg)
def cap(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Captured '+name)
def step(p,delay=.5):s.update(phase=p,next=unreal.GameplayStatics.get_time_seconds(s['w'])+delay)
def key(k,down):helper.send_preview_key(s['pc'],k,down)
def press(k):key(k,True);key(k,False)
def place(eye,target):
 pawn=s['pawn'];camera=pawn.get_component_by_class(unreal.CameraComponent);offset=camera.get_world_location()-pawn.get_actor_location()
 pawn.set_actor_location(eye-offset,False,True);pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
 s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def finish(ok,error=''):
 report.update(passed=ok,error=error);(out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
 cleanup={'at':time.monotonic()+3}
 def close(dt):
  if ed.get_game_world() or time.monotonic()<cleanup['at']:return
  unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
 cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
 if s['busy']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<480,'Opening review timed out'
  if s['phase']==0:
   assert levels.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
   editor_actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
   opening=editor_actors.spawn_actor_from_class(unreal.MiningOpening,unreal.Vector())
   for property_name,value in dict(title_texture=unreal.load_asset('/Game/StartRoom/UI/T_OpeningTitle'),seat_eye=unreal.Vector(-260,-160,127),standing_location=unreal.Vector(-260,-70,98),facing=unreal.Rotator(yaw=13),door_title_location=unreal.Vector(466,0,168),door_title_rotation=unreal.Rotator(yaw=180)).items():opening.set_editor_property(property_name,value)
   unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0');helper.start_floating_preview();s['phase']=1;return
  w=ed.get_game_world()
  if not w:return
  s['w']=w
  if unreal.GameplayStatics.get_time_seconds(w)<s['next']:return
  p=s['phase']
  if p==1:
   openings=actors(unreal.MiningOpening)
   if not openings:return
   pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
   if not pc or not pawn or not openings[0].get_editor_property('world_title').is_visible():return
   s.update(pc=pc,pawn=pawn,opening=openings[0],room=actors(unreal.MiningStartRoom)[0],door=actors(unreal.MainGameDoor)[0])
   s['opening'].start_game();step(2,3.8);return
  if p==2:
   cap('01-logo-retained')
   room=s['room'].get_actor_transform();s['room_t']=room
   check(s['door'].get_actor_location().distance(room.transform_location(unreal.Vector(-14,-200,135)))<10,'Door uses room-local coordinates exactly once')
   place(room.transform_location(unreal.Vector(-200,-200,145)),room.transform_location(unreal.Vector(-14,-200,145)))
   step(3);return
  if p==3:
   check(str(s['pc'].interaction_hud.interaction_action)=='开门','Shared E prompt targets authored door')
   check(s['door'].door_mesh.get_editor_property('render_custom_depth'),'Door has the standard white selection outline')
   cap('02-door-E-outline');press('E');step(4,.4);return
  if p==4:
   check(s['door'].is_open,'E opens the door');cap('03-logo-flight');step(5,1.1);return
  if p==5:
   check(not s['opening'].get_editor_property('world_title').is_visible(),'Flying title is replaced by a projected texture')
   t=s['room_t'];capsule=s['pawn'].get_component_by_class(unreal.CapsuleComponent)
   h=unreal.SystemLibrary.capsule_trace_single(w,t.transform_location(unreal.Vector(-180,-200,130)),t.transform_location(unreal.Vector(100,-200,130)),capsule.get_scaled_capsule_radius(),capsule.get_scaled_capsule_half_height(),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
   report['bedroom_sweep']=str(h.to_tuple() if h else None)
   check(not h or not h.to_tuple()[0],'First bedroom door admits player capsule into little cave')
   decals=s['opening'].get_components_by_class(unreal.DecalComponent)
   check(len(decals)==1,'Exactly one mineable wall projection')
   place(s['room_t'].transform_location(unreal.Vector(220,0,160)),s['room_t'].transform_location(unreal.Vector(525,0,168)))
   cap('04-title-on-wall')
   pickup=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
   pos=pickup.get_actor_location();place(pos+unreal.Vector(-130,0,45),pos);step(6);return
  if p==6:
   press('E');check(s['room'].pickaxe_unlocked,'Pick up the actual starter pickaxe through E')
   s['wall']=next(a for a in actors(unreal.DiggableRock) if a.actor_has_tag('BM_StartRoom_ExitBlocker'))
   place(s['room_t'].transform_location(unreal.Vector(330,0,142)),s['room_t'].transform_location(unreal.Vector(525,0,142)))
   step(7);return
  if p==7:key('LeftMouseButton',True);step(8,3);return
  if p==8:
   key('LeftMouseButton',False);check(s['wall'].successful_cuts>0,'Normal mouse mining cuts the wall beneath the title');cap('05-mined-title')
   # Direct excavation below is only a clearance fixture after real input was verified.
   report['clearance_cut']=s['wall'].mine_sphere(s['room_t'].transform_location(unreal.Vector(525,0,125)),220)
   report['clearance_status']=str(s['wall'].get_editor_property('last_status'))
   step(9,.5);return
  if p==9:
   t=s['room_t'];capsule=s['pawn'].get_component_by_class(unreal.CapsuleComponent)
   hit=unreal.SystemLibrary.capsule_trace_single(w,t.transform_location(unreal.Vector(260,0,122)),t.transform_location(unreal.Vector(650,0,122)),capsule.get_scaled_capsule_radius(),capsule.get_scaled_capsule_half_height(),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
   report['exit_sweep']=str(hit.to_tuple() if hit else None)
   check(not hit or not hit.to_tuple()[0],'Open door and excavated exit admit the full player capsule')
   finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
