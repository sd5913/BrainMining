import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback16'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);h=unreal.MiningGUIEditorLibrary
p=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=p.get_editor_property('bThrottleCPUWhenNotForeground');p.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':0,'next':0,'start':time.monotonic(),'busy':False};r={'passed':False}
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<100,'Timeout'
  if s['phase']==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');h.start_floating_preview();s.update(phase=1,next=time.monotonic()+5);return
  w=ed.get_game_world()
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0)
  openings=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)
  if not pc or not openings:return
  menu=h.read_preview_object(openings[0],'Menu')
  if not menu and s['phase']<=2:return
  if s['phase']==1:
   h.resize_preview_viewport(pc,1280,720);s.update(phase=2,next=time.monotonic()+1);return
  if s['phase']==2:
   image=h.read_preview_object(menu,'TitleImage');material=image.get_editor_property('brush').resource_object
   assert isinstance(material,unreal.MaterialInstanceDynamic),str(material)
   assert material.get_editor_property('parent').get_path_name().startswith('/Game/StartRoom/UI/M_OpeningTitlePerspective')
   assert h.capture_preview(pc,str(out/'09_title_perspective.png'))
   r.update(material=material.get_editor_property('parent').get_path_name(),menu_only=True)
   opening=openings[0];opening.start_game();s.update(phase=3,next=time.monotonic()+4.5);return
  pawn=pc.get_controlled_pawn();progress=pawn.get_component_by_class(unreal.MiningProgressionComponent)
  bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent);mine=pawn.get_component_by_class(unreal.MiningComponent)
  wallet=progress.get_wallet()
  def key(k,down=None):
   if down is None:h.send_preview_key(pc,k,True);h.send_preview_key(pc,k,False)
   else:h.send_preview_key(pc,k,down)
  if s['phase']==3:
   assert openings[0].get_editor_property('bFinished')
   before=progress.get_level(0);funds=wallet.get_editor_property('Balance');minerals=str(wallet.get_editor_property('Minerals'))
   key('F10');assert progress.get_level(0)==before,'F10 alone must not grant tools'
   s['funds']=funds;s['minerals']=minerals
   key('LeftControl',True);s.update(phase=30,next=time.monotonic()+.15);return
  if s['phase']==30:
   funds=s['funds'];minerals=s['minerals']
   key('F10');key('LeftControl',False)
   assert progress.get_level(0)==4,'Ctrl+F10 max pickaxe'
   assert mine.is_drill_unlocked() and bomb.is_skill_unlocked(),'Ctrl+F10 grants both abilities'
   assert wallet.get_editor_property('bDrillSkillUnlocked') and wallet.get_editor_property('bBombSkillUnlocked')
   assert wallet.get_editor_property('Balance')==funds and str(wallet.get_editor_property('Minerals'))==minerals,'Shortcut does not spend resources'
   room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
   assert room.get_editor_property('bPickaxeUnlocked') and not room.get_editor_property('bJetpackUnlocked'),'Only the requested starter tool is granted'
   key('V');s.update(phase=4,next=time.monotonic()+.3);return
  if s['phase']==4:
   assert mine.get_editor_property('bDrillMode'),'Unlocked drill activates through V before jetpack pickup'
   key('G');s.update(phase=5,next=time.monotonic()+.3);return
  if s['phase']==5:
   assert bomb.get_editor_property('bEquipped'),'Unlocked bomb equips through G before jetpack pickup'
   key('G')
   key('RightControl',True);s.update(phase=6,next=time.monotonic()+.15);return
  if s['phase']==6:
   key('F10');key('RightControl',False)
   assert progress.get_level(0)==4 and mine.is_drill_unlocked() and bomb.is_skill_unlocked(),'Right Ctrl also works and repeated grant is stable'
   assert h.capture_preview(pc,str(out/'10_ctrl_f10_tools.png'))
   r.update(passed=True,shortcut='Ctrl+F10',max_pickaxe_level=4,drill_unlocked=True,bomb_unlocked=True,starter_lock_released=True,no_resources_spent=True,left_and_right_ctrl=True)
   finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
def finish(error=None):
 if error:r.update(error=error);unreal.log_error(error)
 (out/'title_perspective.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
 p.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
