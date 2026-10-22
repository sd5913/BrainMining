"""Visual and rendering probes in real MainGame rooms; frame intervals are editor measurements."""
import unreal,json,time,traceback,math
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Polish23'
H=unreal.MiningGUIEditorLibrary;ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic(),'phase':None,'samples':{},'last':time.monotonic()};r={'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):
 assert ok,label
 r['checks'].append(label);unreal.log('POLISH23_VISUAL '+label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def shot(name):check(H.capture_preview(s['pc'],str(out/(name+'.png'))),'Capture '+name)
def pose(eye,target):
 offset=s['pawn'].get_actor_eyes_view_point()[0]-s['pawn'].get_actor_location();s['pawn'].set_actor_location(eye-offset,False,True);s['move'].stop_movement_immediately();s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def run():
 levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview();yield 20
 s['w']=ed.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pawn']=unreal.GameplayStatics.get_player_pawn(s['w'],0);s['move']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent)
 actors(unreal.MiningOpening)[0].start_game();yield 4
 room=actors(unreal.MiningStartRoom)[0]
 for pickup in actors(unreal.MiningStarterPickup):
  p=pickup.get_actor_location();pose(p+unreal.Vector(-125,0,50),p);yield .25
  check(room.interact(s['pc']),'Collect starter equipment')
 r['viewport']=list(s['pc'].get_viewport_size());r['measurement']='PIE Slate callback interval; includes editor work, not isolated GPU time'
 # Exercise real chunked mine serialization, reset, then restore the same mesh.
 field=actors(unreal.MiningTreasureField)[0]
 placement=next(p for p in field.get_editor_property('authored_placements') if any(isinstance(h,unreal.GeneratedMiningRock) for h in p.hosts))
 rock=next(h for h in placement.hosts if isinstance(h,unreal.GeneratedMiningRock))
 normal=placement.surface.transform_location(unreal.Vector(1,0,0))-placement.surface.translation
 pose(placement.surface.translation+normal*250,placement.surface.translation)
 check(rock.mine_sphere(placement.surface.translation-normal*25,110),'Excavate actual chunked mine');yield 4
 cuts=rock.get_editor_property('successful_cuts');triangles=rock.get_editor_property('triangle_count');check(cuts>0,'Chunk excavation commits')
 gi=unreal.GameplayStatics.get_game_instance(s['w']);save=next(o for o in unreal.ObjectIterator(unreal.MiningSaveSubsystem) if o.get_outer()==gi)
 check(save.save_progress(),'Save edited chunks');rock.reset_rock();check(rock.get_editor_property('successful_cuts')==0,'Reset chunk test fixture')
 check(save.load_progress(),'Restore edited chunks');check(rock.get_editor_property('successful_cuts')==cuts and rock.get_editor_property('triangle_count')==triangles,'Chunk geometry and cut counts persist')
 check(H.prepare_collection_preview_area(s['pc'],1,False),'Enter office')
 actors(unreal.MiningExhibitHall)[0].enter_area(1,True)
 office=actors(unreal.PopupCloseStage)[0];office.start_office_round();device=next(d for d in actors(unreal.PopupOfficeDevice) if d.get_editor_property('device_index')==0);pose(device.get_actor_location()-device.get_actor_forward_vector()*215+unreal.Vector(0,0,158),device.get_actor_location()+unreal.Vector(0,0,124));s['pc'].set_view_target_with_blend(s['pawn'],0);yield 3
 devices=actors(unreal.PopupOfficeDevice);surfaces=[a.get_component_by_class(unreal.WidgetComponent) for a in devices]
 check(all(c.get_redraw_time()>=.19 for c in surfaces),'Unfocused office screens throttle redraws')
 s['phase']='office';yield 6;s['phase']=None;shot('office-performance')
 check(H.prepare_collection_preview_area(s['pc'],3,True),'Enter journey')
 stage=actors(unreal.CardRoomStage)[0];center=stage.get_actor_transform().transform_location(unreal.Vector(-75,0,115));pose(center,stage.get_actor_location());yield 2
 interaction=s['pc'].get_component_by_class(unreal.CardRoomInteractionComponent);interaction.interact();yield .5;interaction.interact();yield 5
 for round,n in [(1,1),(2,1),(3,2),(4,17)]:
  for i in range(n):interaction.toggle_card_selection(i)
  interaction.confirm_cards();s['phase']='card_round_'+str(round);yield 1.5;shot('cards-round-'+str(round));yield 3.7;s['phase']=None
 interaction.leave_seat()
 check(H.prepare_collection_preview_area(s['pc'],2,False),'Enter bomb-wall mine');actors(unreal.MiningExhibitHall)[0].enter_area(2,True);yield 1
 walls=[a for a in actors(unreal.DiggableRock) if a.actor_has_tag('BM_BombOnly')]
 wall=next(a for a in walls if a.actor_has_tag('BM_Feedback_ThinBlastLayer'));center,extent=wall.get_actor_bounds(False)
 pose(center-unreal.Vector(extent.x+100,0,0),center);yield 1.5
 check(wall.get_component_by_class(unreal.DecalComponent),'Actual bomb model icon mounted on wall')
 sign=wall.get_component_by_class(unreal.DecalComponent);contact=sign.get_world_location();pose(contact-sign.get_forward_vector()*150,contact);yield .5
 shot('bomb-wall')
 H.send_preview_key(s['pc'],'LeftMouseButton',True);yield 1;H.send_preview_key(s['pc'],'LeftMouseButton',False)
 gi=unreal.GameplayStatics.get_game_instance(s['w']);subtitle=next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==gi)
 mining=s['pawn'].get_component_by_class(unreal.MiningComponent);r['bomb_diagnostics']={'status':mining.status,'target':str(mining.target_rock),'reach':mining.reach,'active_area':actors(unreal.MiningExhibitHall)[0].active_area,'subtitle':str(subtitle.get_active_text())}
 check(str(subtitle.get_active_text())=='这堵墙很硬，用炸弹试试','Pickaxe gives bomb-wall hint');shot('bomb-wall-hint')
 # Authored credits widget plus the native text binding.
 levels.editor_request_end_play();yield 3
 levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');H.start_floating_preview();yield 15
 s['w']=ed.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0)

 d=actors(unreal.EndingEmergence)[0]
 H.write_preview_state(d,'GateIndex','2');H.write_preview_state(d,'CurrentNeuron','0');H.write_preview_state(d,'PossessionCount','1')
 cam=H.read_preview_object(d,'Camera');point=d.neuron_world_position(0)+unreal.Vector(0,0,380)
 cam.set_actor_location(point,False,True);cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(point,d.neuron_world_position(1)),False);s['pc'].set_view_target_with_blend(cam,0);yield 1
 shot('neuron-space-hint')
 check(H.prepare_ending_credits_preview(s['pc'],[1]*12),'Open credits');yield 10
 d=actors(unreal.EndingEmergence)[0];ui=H.read_preview_object(d,'CreditsWidget');thanks=H.read_preview_object(ui,'ThankYou')
 check(str(thanks.get_text())=='恭喜你挖出来了！《挖哦》也因此诞生！感谢游玩！','New credits text');shot('credits')
def finish(error=None):
 r['passed']=error is None;r['error']=error;r['frames']={k:{'n':len(v),'mean_ms':sum(v)/len(v),'p95_ms':sorted(v)[math.ceil(len(v)*.95)-1]} for k,v in s['samples'].items() if v}
 (out/'visuals.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
 if error:unreal.log_error(error)
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play();unreal.SystemLibrary.quit_editor()
gen=run()
def tick(dt):
 now=time.monotonic();gap=(now-s['last'])*1000;s['last']=now
 if s['phase']:s['samples'].setdefault(s['phase'],[]).append(gap)
 if s['busy'] or now<s['next']:return
 s['busy']=True
 try:
  assert now-s['start']<240,'Timeout'
  delay=next(gen);s['next']=time.monotonic()+delay
 except StopIteration:finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
