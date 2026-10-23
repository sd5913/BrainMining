"""Real MainGame PIE regression. Use -MiningSaveSlot=Polish23Test to isolate test saves."""
import unreal,json,time,traceback,math
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Polish23';out.mkdir(parents=True,exist_ok=True)
H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};report={'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):
 assert ok,label
 report['checks'].append(label);unreal.log('POLISH23 '+label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def tag(name):return unreal.GameplayStatics.get_all_actors_with_tag(s['w'],name)
def sub(cls):
 gi=unreal.GameplayStatics.get_game_instance(s['w']);return next(o for o in unreal.ObjectIterator(cls) if o.get_outer()==gi)
def key(name):H.send_preview_key(s['pc'],name,True);H.send_preview_key(s['pc'],name,False)
def pose(eye,target):
 offset=s['pawn'].get_actor_eyes_view_point()[0]-s['pawn'].get_actor_location()
 s['pawn'].set_actor_location(eye-offset,False,True);s['move'].stop_movement_immediately();s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def bind():
 s['w']=ed.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pawn']=unreal.GameplayStatics.get_player_pawn(s['w'],0);s['move']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent)
 s['hall']=actors(unreal.MiningExhibitHall)[0];s['opening']=actors(unreal.MiningOpening)[0];s['room']=actors(unreal.MiningStartRoom)[0]
 s['subtitle']=sub(unreal.SubtitleSubsystem);s['save']=sub(unreal.MiningSaveSubsystem);s['wallet']=sub(unreal.MiningWalletSubsystem)
def shot(name):check(H.capture_preview(s['pc'],str(out/(name+'.png'))),'Capture '+name)
def run():
 check(levels.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load production map');H.start_floating_preview();yield 20
 bind();s['opening'].start_game();yield 4
 check(s['opening'].get_editor_property('bFinished'),'Opening completes')
 for name,text,offset in [
 ('Computer','这次gamejam的主题是“涌现”。嗯……得想一想……',(-130,0,30)),
 ('Book','想到好的点子就记下来吧！',(-90,-70,65)),
 ('Plant','别转悠啦！这里只是休息室，该出门开开脑洞了！',(70,-60,40)),
 ('BookLeft','纸上得来终觉浅',(0,-140,30)),
 ('BookRight','绝知此事要躬行！',(0,-140,30)),
 ('Toilet','谢谢你。',(-100,-100,60))]:
  a=tag('StartRoomView_'+name)[0];center=a.get_actor_bounds(False)[0]
  if name=='Computer':center=unreal.SystemLibrary.get_component_bounds(a.get_component_by_class(unreal.SkeletalMeshComponent))[0]
  pose(center+unreal.Vector(*offset),center);yield .3
  check(s['room'].interact(s['pc']),'Interact '+name)
  check(str(s['subtitle'].get_active_text())==text,'Exact subtitle '+name)
  for i in range(20):s['room'].interact(s['pc'])
  check(s['subtitle'].get_pending_segment_count()==0,'No queued repeated clicks '+name)
  if name=='Book':
   yield 4
   check(str(s['subtitle'].get_active_text())=='按C可以查看灵感藏品','Notebook follow-up')
  if name=='BookLeft':shot('book-left')
 s['subtitle'].stop_all();key('C');yield .5
 check(str(s['subtitle'].get_active_text())=='挖掘就能找到灵感啦~','Empty catalogue subtitle')
 shot('empty-collection');key('C');yield .3
 # Reach the upper junction (second soil wall) and exercise the persistent guide.
 junction=next(a for a in actors(unreal.DiggableRock) if a.actor_has_tag('BM_MainGame_StartRoomJunction'))
 H.write_preview_state(s['room'],'bPickaxeUnlocked','True')
 center=junction.get_actor_bounds(False)[0];pose(center+unreal.Vector(-450,0,40),center);yield .7
 check(s['save'].tutorial_active,'Second soil layer activates guidance')
 check(str(s['subtitle'].get_active_text())=='没思路的话就看看左侧引导栏吧','Guidance subtitle')
 shot('guidance-start')
 check(s['wallet'].add_mineral('2',1),'Acquire a copper ore');yield .6
 check(not(s['save'].tutorial_completed&1),'Wallet fixture is not a physical collection')
 # Seed historical progress for this legacy persistence fixture. Real pickups are
 # covered by verify_packaged25.py, including exposure without collection.
 H.write_preview_state(s['save'],'TutorialCompleted','3')
 shot('guidance-check')
 H.write_preview_state(s['wallet'],'Minerals','()');yield 1.5
 check(s['save'].tutorial_completed&1,'Spending minerals cannot undo completed objective')
 # Progress and terrain snapshot, then a completely new PIE world.
 H.write_preview_state(s['wallet'],'Minerals','((Copper,100),(Iron,100),(Malachite,100),(Diamond,100))')
 s['subtitle'].stop_all();yield 1
 check(str(s['subtitle'].get_active_text())=='材料充足！按[Tab]升级矿镐','Upgrade affordability prompt')
 H.write_preview_state(s['wallet'],'UpgradeLevels','(2,0,0,0)');H.write_preview_state(s['wallet'],'bBombSkillUnlocked','True')
 H.write_preview_state(s['hall'].collection,'Inventory','(0,1,0,1,0,0,0,0,0,0,0,0)')
 rock=tag('BM_StartRoom_ExitBlocker')[0];s['rockname']=rock.get_name();center,extent=rock.get_actor_bounds(False)
 rock.mine_sphere(center-unreal.Vector(extent.x-15,0,0),90);yield 3
 s['triangles']=rock.get_editor_property('triangle_count');s['cuts']=rock.get_editor_property('successful_cuts');check(s['cuts']>0,'Actual terrain excavation committed')
 pose(s['room'].get_actor_location()+unreal.Vector(-260,-70,170),center);yield .3
 check(s['save'].save_progress(),'Save inventory and edited terrain to isolated slot')
 check(s['save'].has_save(),'Saved slot is loadable')
 levels.editor_request_end_play();yield 3
 H.start_floating_preview();yield 18
 bind();check(s['save'].has_save(),'Continue available in a fresh session')
 s['opening'].continue_saved_game();yield 1
 check(s['opening'].get_editor_property('bFinished'),'Restore snapshot in a fresh world')
 check(s['wallet'].get_editor_property('upgrade_levels')[0]==2,'Upgrade persists')
 check(s['save'].tutorial_active and s['save'].tutorial_completed==15,'Guidance progress persists')
 check(s['wallet'].get_editor_property('bBombSkillUnlocked'),'Bomb ownership persists')
 check(s['hall'].collection.inventory[1]==1 and s['hall'].collection.inventory[3]==1,'Collectibles persist')
 rock=tag('BM_StartRoom_ExitBlocker')[0]
 report['terrain_counts']=[s['cuts'],rock.get_editor_property('successful_cuts')]
 check(rock.get_editor_property('successful_cuts')==s['cuts'],'Edited terrain restores after world recreation')
 check(rock.get_editor_property('triangle_count')==s['triangles'],'Excavated mesh topology persists')
 # Release the introductory camera/input without starting a new run.
 # Fall far outside the terrain and verify automatic section-entry recovery.
 s['pc'].set_view_target_with_blend(s['pawn'],0)
 s['pawn'].set_actor_location(s['hall'].areas[0].entry.translation+unreal.Vector(0,90000,0),False,True)
 s['move'].set_movement_mode(unreal.MovementMode.MOVE_FALLING);s['move'].velocity=unreal.Vector(0,0,-1200)
 yield 4.2
 entry=s['hall'].areas[s['hall'].active_area].entry.translation
 check((s['pawn'].get_actor_location()-entry).length()<1000,'Void fall returns to current section entrance')
 check(str(s['subtitle'].get_active_text())=='呼，刚才想到什么来着？','Void recovery subtitle')
 shot('void-recovery')
 for area,count in [(1,1),(3,2)]:
  check(H.prepare_collection_preview_area(s['pc'],area,False),'Enter prototype area '+str(area));s['hall'].enter_area(area,True);yield 1
  report['prototype_'+str(count)]={'active_area':s['hall'].active_area,'found':s['save'].prototypes_found}
  check(s['save'].prototypes_found==count,'Prototype discovered '+str(count))
 stage=actors(unreal.CardRoomStage)[0];center=stage.get_actor_transform().transform_location(unreal.Vector(-75,0,115))
 pose(center,stage.get_actor_location());yield 1
 check(s['save'].prototypes_found==3,'Entering card room discovers prototype three')
 check(H.prepare_collection_preview_area(s['pc'],5,False),'Enter museum');s['hall'].enter_area(5,True);yield 1
 check(s['save'].prototypes_found==4,'Museum discovers prototype four')

 report['passed']=True

gen=run()
def finish(error=None):
 report.update(passed=error is None,error=error)
 (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 if error:unreal.log_error(error)
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 unreal.SystemLibrary.quit_editor()
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<240,'Runtime verification timeout'
  delay=next(gen);s['next']=time.monotonic()+delay
 except StopIteration:finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
