"""Card-room purchase hit test and TV comfort preview in isolated linked-room PIE (same shared progression purchase path)."""
import unreal,time,json,traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
out=Path(unreal.Paths.project_saved_dir())/'Validation/CardComfort';out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
s={'phase':0,'start':time.monotonic(),'next':0};report={'passed':False,'checks':[]}
def check(v,label):
 if not v:raise AssertionError(label)
 report['checks'].append(label)
def shot(n):check(helper.capture_preview(s['pc'],str(out/n)),'Captured '+n)
def pose(p,t):
 s['pawn'].set_actor_location(p,False,True)
 s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],t))
def finish(ok,reason):
 report.update(passed=ok,reason=reason,elapsed=time.monotonic()-s['start'])
 (out/'interaction.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 unreal.log('CARD_COMFORT '+str(ok)+' '+reason)
 levels.editor_request_end_play();s.update(phase=99,next=time.monotonic()+2)
def purchase(index,level):
 check(s['progression'].purchase(index,level),'UI request purchases through shared progression API')
 s['pc'].mining_gui.set_item_level(index,s['progression'].get_level(index),False)
def tick(dt):
 if s.get('busy'):return
 s['busy']=True
 try:
  now=time.monotonic()
  if now<s['next']:return
  phase=s['phase']
  if phase==99:
   unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
  if now-s['start']>180:raise RuntimeError('Timeout at phase '+str(phase))
  if phase==0:helper.start_floating_preview();s.update(phase=1,next=now+2);return
  w=editor.get_game_world()
  if not w:return
  if phase==1:
   pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0);card=pc.get_editor_property('card_interaction');stage=card.get_editor_property('stage')
   if not stage:return
   progression=pawn.get_component_by_class(unreal.MiningProgressionComponent);check(progression.initialize_progression(),'Shared progression initializes in linked-room fixture');wallet=progression.get_wallet();wallet.add_coins(10000)
   # The full-campaign hall normally owns this delegate. Reuse its exact purchase API in a lighter PIE map.
   pc.set_mining_gui_enabled(True)
   pc.mining_gui.on_upgrade_requested.add_callable(purchase)
   pc.mining_gui.pickaxe_upgrade.set_editor_property('bPreviewUpgrades',False)
   s.update(w=w,pc=pc,pawn=pawn,card=card,stage=stage,progression=progression,wallet=wallet,move=pawn.get_component_by_class(unreal.CharacterMovementComponent))
   pose(unreal.MathLibrary.transform_location(stage.get_actor_transform(),unreal.Vector(-270,0,115))+unreal.Vector(0,0,100),stage.get_actor_location())
   s['move'].disable_movement();s.update(phase=2,next=now+1);return
  pc=s['pc'];pawn=s['pawn'];stage=s['stage']
  if phase==2:
   helper.send_preview_key(pc,'Tab',True);s.update(phase=3,next=now+.6);return
  if phase==3:
   check(pc.get_editor_property('bUpgradeMenuHeld'),'Tab opens upgrade panel inside card room')
   gui=pc.mining_gui;s.update(gui=gui,balance=s['wallet'].get_editor_property('balance'),level=s['progression'].get_level(0),price=s['progression'].get_next_price(0))
   button=gui.pickaxe_upgrade.coin_button
   check(helper.simulate_preview_button(button,True),'Upgrade button receives actual Slate hit test through card overlay')
   check(helper.simulate_preview_button(button,False),'Upgrade button accepts click')
   s.update(phase=4,next=now+.3);return
  if phase==4:
   check(s['progression'].get_level(0)==s['level']+1,'Purchase increases real progression level')
   check(s['wallet'].get_editor_property('balance')==s['balance']-s['price'],'Purchase charges exactly configured price')
   shot('01-upgrade-click.png')
   check(helper.send_preview_focused_key(pc,'Tab',False),'Tab release closes upgrade panel after clicking')
   check(not pc.get_editor_property('bUpgradeMenuHeld') and not pc.is_look_input_ignored(),'Upgrade release restores camera input')
   p=pawn.get_actor_location();helper.send_preview_key(pc,'R',True);helper.send_preview_key(pc,'R',False)
   s.update(position=p,phase=5,next=now+.2);return
  if phase==5:
   check((pawn.get_actor_location()-s['position']).length()<1,'R does not restart or teleport in card room')
   s['card'].interact();check(pc.is_move_input_ignored(),'Card match still seats normally')
   helper.send_preview_key(pc,'Q',True);helper.send_preview_key(pc,'Q',False)
   s.update(phase=6,next=now+.2);return
  if phase==6:
   check(pc.is_move_input_ignored(),'Q no longer leaves the card match')
   shot('02-card-controls.png')
   e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0];s['escape']=e
   e.call_method('OnRewardClaimed',args=(stage.get_editor_property('room_id'),));e.retry_escape()
   s['move'].disable_movement();pose(e.get_editor_property('computer_checkpoint_position'),e.get_editor_property('computer_entry'))
   s.update(phase=7,next=now+.3);return
  if phase==7:
   e=s['escape'];entry=e.get_editor_property('computer_entry');axis=e.get_editor_property('tunnel_end')-entry;axis=axis/axis.length();s['axis']=axis;s['entry']=entry
   rocks=list(e.get_editor_property('computer_rocks'));s['rocks']=rocks
   for rock in rocks[:5]:
    for a in [-200,-100,0,100,200]:rock.mine_sphere(rock.get_actor_location()+axis*a,265)
   pose(entry+axis*900,entry+axis*1900)
   s.update(phase=8,next=now+2);return
  if phase==8:
   shot('03-tv-stable-guide.png')
   mining=pawn.get_component_by_class(unreal.MiningComponent);s['mining']=mining
   target=s['rocks'][5].get_actor_location();pose(target-s['axis']*460,target)
   s.update(phase=9,next=now+.4);return
  if phase==9:
   check(s['mining'].activate_drill(),'Original drill activates in television')
   s['mining'].try_mine();s.update(phase=10,next=now+.08);return
  if phase==10:
   bursts=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningDrillDustBurst)
   check(bool(bursts),'Real drill cut produces contact particles')
   tint=bursts[-1].get_editor_property('dust_tint');check(tint.b>tint.r,'Card-room dust uses cool neutral palette')
   sparks=bursts[-1].get_editor_property('spark_sprites');mat=sparks.get_material(0)
   tint=mat.get_vector_parameter_value('SparkTint');check(tint.b>tint.r,'Card-room sparks no longer use orange palette')
   check(mat.get_scalar_parameter_value('Intensity')<=3,'Spark brightness is restrained')
   shot('04-tv-drill-contact.png');s.update(phase=11,next=now+.5);return
  if phase==11:finish(True,'Real card-room upgrade purchase, input cleanup and TV contact appearance verified')
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
