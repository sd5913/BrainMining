"""PIE visual checks for pause/collection, museum feedback, echo entrance and sky."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/CollectionFeedback';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'next':0,'start':time.monotonic(),'checks':[],'images':[],'views':[]}
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def presence(mirror):return next(a for a in actors(unreal.Actor) if a.get_owner()==mirror)
def check(name,v):
 assert v,name
 s['checks'].append(name)
def pose(p,target):
 move=s['pawn'].get_component_by_class(unreal.CharacterMovementComponent);move.stop_movement_immediately();move.disable_movement()
 s['pawn'].set_actor_location(p,False,True);s['pc'].set_view_target_with_blend(s['pawn'],0);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target));s['expected_pose']=list(p.to_tuple())
def shot(name):
 assert unreal.MiningGUIEditorLibrary.capture_preview(s['pc'],str(OUT/(name+'.png')))
 s['images'].append(name)
 s['views'].append(dict(name=name,expected=s.get('expected_pose'),actual=list(s['pawn'].get_actor_location().to_tuple()),camera=list(s['pc'].player_camera_manager.get_camera_location().to_tuple()),view_target=s['pc'].get_view_target().get_name()))
def area(i):assert unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],i,i==3)
def finish(ok,error=''):
 s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 (OUT/'rooms.json').write_text(json.dumps(dict(passed=ok,error=error,checks=s['checks'],images=s['images'],views=s['views']),ensure_ascii=False,indent=2),encoding='utf-8')
 clean={'next':time.monotonic()+2,'loaded':False}
 def close(dt):
  if ed.get_game_world() or time.monotonic()<clean['next']:return
  if not clean['loaded']:s.clear();levels.load_level('/Engine/Maps/Entry');clean.update(loaded=True,next=time.monotonic()+1);return
  unreal.unregister_slate_post_tick_callback(clean['handle']);unreal.SystemLibrary.quit_editor()
 clean['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
 if s.get('busy') or s.get('done'):return
 s['busy']=True
 try:
  now=time.monotonic();ph=s['phase']
  if now-s['start']>420:raise RuntimeError('Timed out at '+str(ph))
  if now<s['next']:return
  if ph==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0');unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,next=now+5);return
  if ph==1:
   w=ed.get_game_world()
   if not w:return
   s['w']=w;hall=actors(unreal.MiningExhibitHall)[0]
   if not hall.ready:return
   for a in actors(unreal.MiningOpening):a.start_game()
   s.update(hall=hall,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=2,next=now+5);return
  if ph==2:
   for a in actors(unreal.MiningOpening):
    if not a.finished:a.start_game();s['next']=now+.5;return
   if not s.get('stain_reviewed'):
    area(0);field=actors(unreal.MiningCollectionField)[0];p=next(p for p in field.authored_placements if p.item_id==2);front=p.clue_transform.translation;normal=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(-1,0,0))
    pose(front+normal*230+unreal.Vector(0,0,-30),front);s.update(phase=23,next=now+2);return
   area(1);s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   instance=unreal.GameplayStatics.get_game_instance(s['w']);menu=next(o for o in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if o.get_outer()==instance);s['menu']=menu
   field=actors(unreal.MiningCollectionField)[0];s['field']=field;tape=next(i for i in field.items if i.item_id==5);p=tape.get_actor_location();pose(p+unreal.Vector(35,125,65),p);s.update(phase=20,next=now+1);return
  if ph==23:shot('reagent_stain_review');s.update(stain_reviewed=True,phase=2,next=now);return
  if ph==20:
   s['field'].collect_target();check('Catalogue owns a real collected cassette',s['field'].inventory[5]==1)
   check('Pause menu opens',s['menu'].toggle_pause_menu());check('Pause stops world',unreal.GameplayStatics.is_game_paused(s['w']));s.update(phase=3,next=now+1);return
  if ph==3:
   shot('pause_menu');s['menu'].request_quit_confirmation();check('Quit confirmation active',s['menu'].quit_confirmation);s.update(phase=4,next=now+1);return
  if ph==4:
   shot('quit_confirmation');s['menu'].cancel_quit_confirmation();check('Cancel returns to pause',not s['menu'].quit_confirmation)
   field=s['field']
   s['menu'].open_pause_collection();check('Pause opens collection',field.catalogue_open);s.update(phase=5,next=now+2);return
  if ph==5:
   shot('collection_owned_and_locked');s['field'].toggle_catalogue();check('Catalogue resumes world',not unreal.GameplayStatics.is_game_paused(s['w']));area(5)
   s['puzzle']=actors(unreal.WenSiMirrorPuzzle)[0];s['exhibits']={a.get_editor_property('kind'):a for a in actors(unreal.WenSiMirrorExhibit)}
   ball=s['exhibits'][3];p=s['puzzle'].get_actor_location();bp=ball.get_actor_location();check('Balloon touches ceiling',abs(bp.z+55-p.z-420)<5)
   pose(p+unreal.Vector(-650,-100,100),bp);s.update(phase=6,next=now+3);return
  if ph==6:
   shot('museum_ceiling_balloon');ball=s['exhibits'][3];tether=next(c for c in ball.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Part2');check('Balloon tether stays on original plinth',abs(tether.get_world_location().z-tether.get_world_scale().z*50-(s['puzzle'].get_actor_location().z+64))<3)
   cage=s['exhibits'][7];p=cage.get_actor_location();pose(p+unreal.Vector(-320,-45,-30),p+unreal.Vector(0,0,70));check('Cage has warning',any(c.get_name()=='PreciousExhibitWarning' for c in cage.get_components_by_class(unreal.WidgetComponent)));s.update(phase=7,next=now+2);return
  if ph==7:
   shot('museum_gold_cage_warning');mirror=s['exhibits'][10];s['mirror']=mirror;p=mirror.get_actor_location();n=mirror.get_actor_forward_vector();pose(p+n*270-unreal.Vector(0,0,35),p);s.update(phase=8,next=now+2);return
  if ph==8:
   shot('museum_mirror_center');mirror=s['mirror'];reflected=presence(mirror);check('Mirror presence follows pawn',reflected.get_actor_location().distance(s['pawn'].get_actor_location())<1);s['mirror_first']=reflected.get_actor_location();p=mirror.get_actor_location();pose(s['pawn'].get_actor_location()+mirror.get_actor_right_vector()*70,p);s.update(phase=9,next=now+2);return
  if ph==9:
   shot('museum_mirror_moved');mirror=s['mirror'];check('Mirror moves with lateral player motion',presence(mirror).get_actor_location().distance(s['mirror_first'])>60)
   check('Normal puzzle shrink available',s['puzzle'].skip_to_reward());p=mirror.get_actor_location();pose(p+mirror.get_actor_forward_vector()*210+unreal.Vector(0,0,-95),p);s.update(phase=10,next=now+2);return
  if ph==10:
   shot('museum_mirror_small');check('Reflection matches small player scale',abs(presence(s['mirror']).get_actor_scale3d().x-s['pawn'].get_actor_scale3d().x)<.001 and s['pawn'].get_actor_scale3d().x<.5)
   s['puzzle'].reset_puzzle();area(3);echo=actors(unreal.EchoPassageStage)[0];p=echo.get_actor_location();pose(p+unreal.Vector(-610,0,1000),p+unreal.Vector(240,0,1050));s.update(phase=11,next=now+2);return
  if ph==11:
   shot('echo_purple_enclosed_entrance');room=actors(unreal.MiningRoomEscape)[0];p=room.stage_ribbon.get_actor_location();pose(p+unreal.Vector(600,0,130),p+unreal.Vector(1800,0,180));s.update(phase=12,next=now+3);return
  if ph==12:
   shot('tv_stage_exit_lighting');area(4);water=actors(unreal.HeavenToiletWater)[0];f=water.spill_coordinate_frame;s['heaven_frame']=f
   p=unreal.MathLibrary.transform_location(f,unreal.Vector(2580,0,160));pose(p,unreal.MathLibrary.transform_location(f,unreal.Vector(2580,900,650)));s.update(phase=13,next=now+3);return
  if ph==13:
   shot('heaven_curved_sky');f=s['heaven_frame'];pose(unreal.MathLibrary.transform_location(f,unreal.Vector(-530,0,140)),unreal.MathLibrary.transform_location(f,unreal.Vector(-275,125,130)));s.update(phase=14,next=now+3);return
  if ph==14:shot('heaven_faucet_red_rock');finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
