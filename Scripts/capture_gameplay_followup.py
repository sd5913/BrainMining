"""Rendered MainGame checks for echo focus, faucet, pickaxe and shared card reward UI."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/GameplayFollowup'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);H=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s=dict(phase=0,next=0,start=time.monotonic(),checks=[],images=[])
def actors(c):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],c)
def check(v,label):assert v,label;s['checks'].append(label)
def key(k,on=True):H.send_preview_key(s['pc'],k,on)
def shot(name):assert H.capture_preview(s['pc'],str(OUT/(name+'.png')));s['images'].append(name)
def area(i):assert H.prepare_collection_preview_area(s['pc'],i,False)
def pose(p,target):
 m=s['pawn'].get_component_by_class(unreal.CharacterMovementComponent);m.stop_movement_immediately();m.disable_movement()
 s['pawn'].set_actor_location(p,False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target))
def finish(ok,error=''):
 s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
 (OUT/'visual.json').write_text(json.dumps(dict(passed=ok,error=error,checks=s['checks'],images=s['images']),ensure_ascii=False,indent=2),encoding='utf-8')
 if ed.get_game_world():levels.editor_request_end_play()
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
  if now-s['start']>360:raise RuntimeError('Timeout '+str(ph))
  if now<s['next']:return
  if ph==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0');H.start_floating_preview();s.update(phase=1,next=now+5);return
  if ph==1:
   w=ed.get_game_world()
   if not w:return
   s['w']=w;hall=actors(unreal.MiningExhibitHall)[0]
   if not hall.ready:return
   for a in actors(unreal.MiningOpening):a.start_game()
   s.update(pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=2,next=now+5);return
  if ph==2:
   for a in actors(unreal.MiningOpening):
    if not a.finished:a.start_game();s['next']=now+.5;return
   if not s.get('starter'):
    pick=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
    p=pick.get_actor_location();pose(p+unreal.Vector(-100,0,45),p);s.update(phase=20,next=now+.5);return
   area(3);field=actors(unreal.MiningCollectionField)[0];item=next(i for i in field.items if i.item_id==6);echo=actors(unreal.EchoPassageStage)[0]
   p=item.get_actor_location();pose(p+unreal.Vector(0,220,45),p)
   neutral=next(c for c in echo.get_components_by_class(unreal.PostProcessComponent) if c.get_name()=='EchoMonochrome')
   check(neutral.get_editor_property('enabled') and not neutral.get_editor_property('unbound') and neutral.settings.scene_fringe_intensity==0,'Echo has an enabled spatial zero-fringe override')
   echo.emit_echo(p,True);s.update(field=field,item=item,echo=echo,phase=3,next=now+.45);return
  if ph==20:
   check(actors(unreal.MiningStartRoom)[0].interact(s['pc']),'Pick up the actual starter pickaxe');s.update(starter=True,phase=2,next=now+.5);return
  if ph==3:
   check(s['field'].target==s['item'],'Echo collectible still uses normal aim selection');shot('echo-white-focus')
   r=s['pc'].get_control_rotation();s['pc'].set_control_rotation(unreal.Rotator(pitch=r.pitch,yaw=r.yaw+30,roll=r.roll));s['echo'].emit_echo(s['item'].get_actor_location(),True)
   s.update(phase=4,next=now+.45);return
  if ph==4:
   check(s['field'].target!=s['item'],'Looking away clears selection while the echo persists');shot('echo-white-away')
   area(4);faucet=next(a for a in actors(unreal.StaticMeshActor) if a.get_actor_label()=='COL_Heaven_SmallFaucet');p=faucet.get_actor_location();pose(p+unreal.Vector(0,-250,-20),p);s.update(phase=5,next=now+1);return
  if ph==5:
   shot('heaven-connected-faucet');area(0);p=next(p for p in s['field'].authored_placements if p.item_id==4);front=p.clue_transform.translation;n=unreal.MathLibrary.transform_direction(p.clue_transform,unreal.Vector(-1,0,0));pose(front+n*250+unreal.Vector(0,0,-30),front);s.update(phase=6,next=now+1);return
  if ph==6:
   shot('pickaxe-rest');s['effects']=s['pawn'].get_component_by_class(unreal.MiningToolEffectsComponent);key('LeftMouseButton');key('LeftMouseButton',False);s.update(phase=7,next=now+.2);return
  if ph==7:
   shot('pickaxe-strike');progress=s['pawn'].get_component_by_class(unreal.MiningProgressionComponent)
   for material in ['2','4','6','9']:progress.get_wallet().add_mineral(material,100)
   check(progress.purchase(0,progress.get_level(0)+1),'Actual purchase starts the upgrade animation');s.update(phase=8,next=now+.35);return
  if ph==8:
   check(s['effects'].pickaxe_upgrade_remaining>0,'Upgrade celebration remains visible');shot('pickaxe-upgrade');area(3)
   ruler=next(a for a in actors(unreal.StaticMeshActor) if a.get_actor_label()=='Study V09 / Spiral flight 2')
   check(ruler.get_actor_enable_collision() and ruler.static_mesh_component.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION,'First ruler ramp blocks the player')
   center,extent=ruler.get_actor_bounds(False);pose(center+unreal.Vector(-600,-450,180),center);s.update(phase=9,next=now+1);return
  if ph==9:
   shot('first-ruler-collision');s['stage']=actors(unreal.CardRoomStage)[0]
   unreal.SystemLibrary.execute_console_command(s['w'],'BrainMining.ReviewP2',s['pc']);s.update(phase=10,next=now+.2);return
  if ph==10:
   reward=next(c for c in s['stage'].get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='CollectibleOrb')
   if not reward.is_visible():s['next']=now+.1;return
   check(reward.get_editor_property('render_custom_depth') and reward.get_editor_property('relative_scale3d').x>=2.5,'Card drill is enlarged with the common white outline')
   s.update(phase=11,next=now+.35);return
  if ph==11:
   hud=s['pc'].get_editor_property('interaction_hud');check('钻机' in hud.get_editor_property('interaction_action'),'Victory offers the common E prompt');shot('card-drill-common-ui');key('E');key('E',False);s.update(phase=12,next=now+.8);return
  if ph==12:
   mine=s['pawn'].get_component_by_class(unreal.MiningComponent);check(mine.is_drill_unlocked(),'Real E pickup grants the drill')
   e=actors(unreal.MiningRoomEscape)[0];entry=e.get_editor_property('computer_entry');pose(entry+unreal.Vector(-1100,-400,-60),entry);s.update(escape=e,phase=13,next=now+4);return
  if ph==13:
   if s['escape'].get_editor_property('water_z')<s['escape'].get_editor_property('table_water_z'):s['next']=now+.5;return
   shot('television-green-screen');finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
