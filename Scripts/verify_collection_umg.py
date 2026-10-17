"""Create the editable UMG asset once, exercise real pickup and widget input, restore the user's map."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Automation/CollectionUMG';OUT.mkdir(parents=True,exist_ok=True)
assert unreal.CollectionUMGEditorLibrary.create_collection_inspect_widget()
f=ROOT/'Scripts/fix_collection_umg_fonts.py';exec(compile(f.read_text(encoding='utf-8-sig'),str(f),'exec'),{})
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ED=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert L.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03')
s={'phase':0,'at':time.monotonic(),'start':time.monotonic(),'checks':[]}
def check(label,ok):
 s['checks'].append(dict(label=label,passed=bool(ok)))
 unreal.log('COLLECTION_UMG_CHECK '+label+' '+str(bool(ok)))
def finish(error=None):
 s['error']=error;s['passed']=not error and all(c['passed'] for c in s['checks'])
 (OUT/'validation.json').write_text(json.dumps(s,ensure_ascii=False,indent=2,default=str),encoding='utf-8')
 L.editor_request_end_play();s.update(phase=99,at=time.monotonic())
def tick(dt):
 try:
  now=time.monotonic();age=now-s['start'];phase=s['phase']
  if phase==99:
   if now-s['at']>3:
    unreal.unregister_slate_post_tick_callback(handle)
    if s['passed']:L.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
    asset=unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')
    unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([asset])
    unreal.log('COLLECTION_UMG_FINISHED '+str(s['passed']))
   return
  if age>100:finish('Timed out');return
  if phase==0:
   unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,at=now);return
  w=ED.get_game_world()
  if not w:return
  pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
  fields=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)
  if not pc or not pawn or not fields:return
  f=fields[0]
  if not f.items:return
  widget=f.get_editor_property('catalogue_widget')
  def control(n):return unreal.CollectionUMGEditorLibrary.find_inspect_control(widget,n)
  def key(k):
   unreal.MiningGUIEditorLibrary.send_preview_focused_key(pc,k,True);unreal.MiningGUIEditorLibrary.send_preview_focused_key(pc,k,False)
  def click(n):
   b=control(n);pressed=unreal.MiningGUIEditorLibrary.simulate_preview_button(b,True);released=unreal.MiningGUIEditorLibrary.simulate_preview_button(b,False)
   check('hit test and click '+n,pressed and released)
  def shot(n):unreal.MiningGUIEditorLibrary.capture_preview(pc,str(OUT/n))
  if phase==1 and now-s['at']>2:
   check('runtime uses editable Widget Blueprint',widget is not None and widget.get_class().get_name()=='WBP_CollectionInspect_C')
   item=next(a for a in f.items if a.item_id==2 and a.released and not a.collected)
   pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
   target=item.get_actor_location();pawn.set_actor_location(target+unreal.Vector(-180,0,50),False,True)
   pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target))
   s.update(phase=2,at=now)
  elif phase==2 and now-s['at']>.8:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',True);s.update(phase=3,at=now)
  elif phase==3 and now-s['at']>1:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',False)
   check('real E pickup opens UMG and pauses excavation',f.catalogue_open and f.inventory[2]==1 and pc.show_mouse_cursor and not pawn.get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
   check('owned description bound',str(control('ItemName').get_text())==unreal.MiningCollectible.item_name(2) if hasattr(unreal.MiningCollectible,'item_name') else bool(str(control('ItemName').get_text())))
   shot('01-owned.png');click('ItemButton1');s.update(phase=4,at=now)
  elif phase==4 and now-s['at']>.6:
   check('unknown slot hides preview',f.selected_id==1 and str(control('ItemName').get_text())=='尚未发现' and control('PreviewImage').get_visibility()==unreal.SlateVisibility.HIDDEN)
   shot('02-unknown.png');click('NextButton');s.update(phase=5,at=now)
  elif phase==5 and now-s['at']>.6:
   check('next returns to owned',f.selected_id==2)
   s['rotation_before']=str(f.preview.get_actor_rotation())
   panel=control('PreviewPanel');slot=unreal.WidgetLayoutLibrary.slot_as_canvas_slot(panel);s['panel_position']=slot.get_position();slot.set_position(s['panel_position']+unreal.Vector2D(65,25))
   s.update(phase=6,at=now)
  elif phase==6 and now-s['at']>.6:
   check('drag uses moved preview geometry',unreal.CollectionUMGEditorLibrary.exercise_inspect_pointer(widget,False) and str(f.preview.get_actor_rotation())!=s['rotation_before'])
   s['capture_before']=str(f.get_component_by_class(unreal.SceneCaptureComponent2D).get_world_location())
   check('wheel handled over moved preview',unreal.CollectionUMGEditorLibrary.exercise_inspect_pointer(widget,True))
   check('wheel changes camera distance',str(f.get_component_by_class(unreal.SceneCaptureComponent2D).get_world_location())!=s['capture_before'])
   key('F');s.update(phase=7,at=now)
  elif phase==7 and now-s['at']>.6:
   check('F resets preview rotation',abs(f.preview.get_actor_rotation().yaw+30)<.1)
   key('Left');s.update(phase=8,at=now)
  elif phase==8 and now-s['at']>.6:
   check('keyboard changes selection',f.selected_id==1)
   key('Right');click('PreviousButton');s.update(phase=9,at=now)
  elif phase==9 and now-s['at']>.6:
   check('previous button changes selection',f.selected_id==1)
   unreal.WidgetLayoutLibrary.slot_as_canvas_slot(control('PreviewPanel')).set_position(s['panel_position']);s.pop('panel_position')
   click('CloseButton');s.update(phase=10,at=now)
  elif phase==10 and now-s['at']>.6:
   check('close restores game input and cursor',not f.catalogue_open and not pc.show_mouse_cursor and pawn.get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
   f.toggle_catalogue();s.update(phase=11,at=now)
  elif phase==11 and now-s['at']>.6:
   key('Escape');s.update(phase=12,at=now)
  elif phase==12 and now-s['at']>.6:
   check('Escape closes reopened UMG',not f.catalogue_open);finish()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)



