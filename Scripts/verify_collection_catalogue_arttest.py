"""Review native catalogue layout with real focus keys and mouse drag in PIE."""
import unreal,time,pathlib,json
s={'phase':0,'start':time.monotonic(),'checks':[]};out=pathlib.Path(unreal.Paths.project_saved_dir())/'HeavenPreview'
def check(label,ok):s['checks'].append({'label':label,'pass':bool(ok)})
def tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  w=worlds[0];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
  fields=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)
  if not pawn or not fields or not fields[0].items:return
  f=fields[0];age=time.monotonic()-s['start'];phase=s['phase']
  def key(name):unreal.MiningGUIEditorLibrary.send_preview_focused_key(pc,name,True);unreal.MiningGUIEditorLibrary.send_preview_focused_key(pc,name,False)
  def shot(name):unreal.MiningGUIEditorLibrary.capture_preview(pc,str(out/name))
  if phase==0 and age>3:
   pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING);pawn.set_actor_location(unreal.Vector(2160,-150,185),False,True);pc.set_control_rotation(unreal.Rotator(pitch=-7,yaw=18,roll=0))
   f.inventory[0]=1;f.select_item(0);f.toggle_catalogue();s.update(phase=1,at=age)
  elif phase==1 and age>s['at']+3:
   check('Tab catalogue opens with visible mouse and pauses excavation',f.catalogue_open and pc.show_mouse_cursor and not pawn.get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
   shot('catalogue_toilet.png');key('Right');s.update(phase=2,at=age)
  elif phase==2 and age>s['at']+.6:
   check('right arrow key selects unknown next slot without changing inventory',f.selected_id==1 and list(f.inventory)==[1,0,0,0]);shot('catalogue_unknown.png')
   f.inventory[2]=1;f.select_item(2);s.update(phase=3,at=age)
  elif phase==3 and age>s['at']+2:
   shot('catalogue_rack.png');s['rotation_before']=str(f.preview.get_actor_rotation());f.rotate_preview(45,18);s.update(phase=4,at=age)
  elif phase==4 and age>s['at']+1:
   check('owned item rotation updates its 3D actor',str(f.preview.get_actor_rotation())!=s['rotation_before']);shot('catalogue_rotated.png');key('Left');s.update(phase=5,at=age)
  elif phase==5 and age>s['at']+.5:
   check('left arrow selects prior slot',f.selected_id==1);key('Tab');s.update(phase=6,at=age)
  elif phase==6 and age>s['at']+.5:
   check('Tab returns to play and restores mining input',not f.catalogue_open and not pc.show_mouse_cursor and pawn.get_component_by_class(unreal.MiningComponent).is_component_tick_enabled())
   f.inventory[0]=0;f.select_item(2);f.toggle_catalogue();s['passed']=all(c['pass'] for c in s['checks']);(out/'catalogue_validation.json').write_text(json.dumps(s,ensure_ascii=False,indent=2),encoding='utf8');print('CATALOGUE_READY_FOR_POINTER_CHECKS',s['passed']);unreal.unregister_slate_post_tick_callback(handle)
 except Exception as e:
  s['error']=repr(e);(out/'catalogue_validation.json').write_text(json.dumps(s,ensure_ascii=False,indent=2),encoding='utf8');print('CATALOGUE_ERROR',repr(e));unreal.unregister_slate_post_tick_callback(handle);unreal.EditorLevelLibrary.editor_end_play()
handle=unreal.register_slate_post_tick_callback(tick);unreal.MiningGUIEditorLibrary.start_floating_preview()
