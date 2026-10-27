"""Validate the actual saved MainGame, door arc, stand-up rail and encounter gates."""
import unreal,json,time,traceback,math
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback13';H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};r={'passed':False,'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(v,msg):
 assert v,msg
 r['checks'].append(msg);unreal.log('FEEDBACK13 '+msg)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def shot(name):check(H.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Capture '+name)
def v(p):return [p.x,p.y,p.z]
def run():
 check(H.update_ending_credits_feedback(),'Compile and save editable credits with ThankYou binding')
 check(levels.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load production MainGame');H.start_floating_preview()
 while not ed.get_game_world():yield .2
 s['w']=ed.get_game_world()
 while not actors(unreal.MiningOpening):yield .2
 pc=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pc']=pc;pawn=pc.get_controlled_pawn();m=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
 opening=actors(unreal.MiningOpening)[0]
 while not H.read_preview_object(opening,'Menu'):yield .1
 yield 2;H.resize_preview_viewport(pc,1280,720);yield .5
 opening.start_game();samples=[];frame=0
 while not opening.get_editor_property('bFinished'):
  samples.append([unreal.GameplayStatics.get_time_seconds(s['w'])]+v(pc.player_camera_manager.get_camera_location()))
  H.capture_preview_video_frame(pc,str(OUT/('stand-%04d.jpg'%frame)),85);frame+=1;yield 0
 yield .25;samples.append([unreal.GameplayStatics.get_time_seconds(s['w'])]+v(pc.player_camera_manager.get_camera_location()))
 r['stand_camera']=samples;speeds=[math.dist(a[1:],b[1:])/(b[0]-a[0]) for a,b in zip(samples,samples[1:]) if b[0]>a[0]+.00001];r['stand_max_speed']=max(speeds)
 check(max(speeds)<400,'Stand-up camera is continuous through player handoff');check(samples[-1][3]>samples[0][3]+15,'Player rises from the bed to standing eye height');shot('stand_complete')
 hall=actors(unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
 door=actors(unreal.MainGameDoor)[0];before=unreal.SystemLibrary.get_component_bounds(door.door_mesh)[0];inside=opening.get_editor_property('SeatEye');out=before-inside;out.z=0;out=out.normal();door.open_door();arcs=[]
 for delay in [.12,.18,.2,.45]:
  yield delay;cur=unreal.SystemLibrary.get_component_bounds(door.door_mesh)[0];arcs.append(unreal.MathLibrary.dot_vector_vector(cur-before,out))
 r['door_outward_arc_cm']=arcs;check(min(arcs)>0 and arcs[-1]>25,'Actual door leaf moves outward at every sampled point, relative to bed interior');shot('door_outward')
 seg=unreal.load_asset('/Game/Subtitles/DA_LeaveStartRoom');check(not any('软软的泥土' in str(x.text) for x in seg.get_editor_property('lines')),'Dirt hint is reserved for approaching the soil, not leaving the bedroom')
 room=actors(unreal.MiningStartRoom)[0];pack=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.JETPACK);target=pack.get_actor_location();eye=target+unreal.Vector(-100,0,65);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(eye-offset,False,True);m.disable_movement();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target));yield .3;check(room.interact(pc),'Collect actual starter jetpack before testing encounter flight')
 check(H.prepare_collection_preview_area(pc,3,False),'Enter blind echo area');yield .6
 fs=m.flight_settings;fs.set_editor_property('bEnabled',True);m.flight_settings=fs;m.set_flight_input_held(True)
 check(not m.get_editor_property('bFlightInputHeld'),'Unlocked jetpack cannot engage in blind echo')
 ent=actors(unreal.MiningGiantRoomEntrance)[0];tr=ent.get_actor_transform()
 pawn.set_actor_location(tr.transform_location(unreal.Vector(-180,0,100)),False,True);yield .4;m.set_flight_input_held(True);check(not m.get_editor_property('bFlightInputHeld'),'Flight stays blocked in the passage before the card entrance')
 pawn.set_actor_location(tr.transform_location(unreal.Vector(ent.rock_size.x*.5+180,0,100)),False,True);yield .4;m.set_flight_input_held(True);check(m.get_editor_property('bFlightInputHeld'),'Flight becomes available inside the card room');m.set_flight_input_held(False)
 guides=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'CardRoomLeftRulerGuide');check(len(guides)==18,'Saved entrance ruler has seventeen readable ticks')
 pawn.set_actor_location(unreal.Vector(6830,3480,10105),False,True);m.disable_movement();pc.set_control_rotation(unreal.Rotator(pitch=-18,yaw=-48));pc.set_view_target_with_blend(pawn,0);yield .5;shot('card_left_ruler')
 # All weapon entry points reject protected toilet walls without changing geometry.
 check(H.prepare_collection_preview_area(pc,0,False),'Enter toilet room');yield .4
 walls=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'BM_UnmineableBoundary');check(len(walls)==6,'Six outer toilet boundaries are protected')
 for a in walls:
  p=a.get_actor_location();cuts=a.successful_cuts
  check(not a.mine_sphere(p,90) and not a.mine_pickaxe(unreal.Transform(location=p),unreal.Vector(50,50,50),1,1) and not a.mine_blast(p,90,.3,1) and a.successful_cuts==cuts,'All mining modes reject '+a.get_actor_label())
 r['passed']=True
def finish(error=None):
 if error:r.update(passed=False,error=error);unreal.log_error(error)
 (OUT/'main_runtime.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 unreal.SystemLibrary.quit_editor()
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<160,'Timeout'
  s['next']=time.monotonic()+next(gen)
 except StopIteration:finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
