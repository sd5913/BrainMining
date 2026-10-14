"""In PIE, validate puzzle rules and select both bubbles via real E/G input.

Launch in Lvl_WenSi or Lvl_WenSi_MirrorMuseum, then inspect / drag with Slate MCP.
This does not save gameplay maps.
"""
import unreal,time,json,pathlib
thought_state={'start':time.monotonic(),'phase':0,'checks':[]}
thought_out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/WenSiThoughtUI';thought_out.mkdir(parents=True,exist_ok=True)
def thought_tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn:return
  age=time.monotonic()-thought_state['start'];phase=thought_state['phase']
  mirror=bool(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorPuzzle))
  puzzle=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorPuzzle if mirror else unreal.WenSiPuzzle)[0]
  thought_state['variant']='mirror' if mirror else 'basic'
  if phase==0 and age>3:
   result=puzzle.validate_mirror_puzzle() if mirror else puzzle.validate_puzzle()
   thought_state['checks'].append({'label':'existing puzzle mechanics','pass':result.startswith('PASS'),'result':result})
   pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
   thought_state['phase']=1;thought_state['at']=age
  elif phase in (1,3) and age>thought_state['at']+.4:
   kind=6 if mirror and phase==1 else 3 if mirror else 0 if phase==1 else 1
   obj=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorExhibit if mirror else unreal.WenSiObject) if a.get_editor_property('kind' if mirror else 'object_kind')==kind)
   center=obj.get_actor_bounds(False)[0]
   # The independent puzzle pawn's camera is 65cm above its capsule center.
   position=center+unreal.Vector(-210,25 if not mirror and phase==1 else 0,-65)
   pawn.set_actor_location(position,False,True);pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0))
   thought_state['phase']=phase+1;thought_state['at']=age
  elif phase in (2,4) and age>thought_state['at']+.7:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',True);thought_state['release']='E';thought_state['phase']=phase+1;thought_state['at']=age
  elif phase==5 and age>thought_state['at']+.8:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'G',True);thought_state['release']='G';thought_state['phase']=6;thought_state['at']=age
  elif phase==6 and age>thought_state['at']+.8:
   path=thought_out/(thought_state['variant']+'.png');unreal.MiningGUIEditorLibrary.capture_preview(pc,str(path))
   thought_state['checks'].append({'label':'G opens puzzle editing cursor','pass':pc.get_editor_property('show_mouse_cursor')})
   (thought_out/(thought_state['variant']+'.json')).write_text(json.dumps(thought_state,ensure_ascii=False,indent=2),encoding='utf8')
   unreal.unregister_slate_post_tick_callback(thought_handle);print('WENSI_THOUGHT_READY_FOR_DRAG',thought_state['variant'])
  if thought_state.get('release') and age>thought_state['at']+.15:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,thought_state.pop('release'),False)
 except Exception as error:
  thought_state['error']=repr(error);(thought_out/'error.json').write_text(json.dumps(thought_state,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(thought_handle);print('WENSI_UI_ERROR',repr(error))
thought_handle=unreal.register_slate_post_tick_callback(thought_tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
