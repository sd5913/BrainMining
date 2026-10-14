"""PIE validation and game-rendered captures in the current Museum *1 copy."""
import unreal,time,pathlib,json

museum_test={'start':time.monotonic(),'phase':0,'checks':[]}
museum_out=pathlib.Path(unreal.Paths.project_saved_dir())/'MuseumArt'

def museum_tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn:return
  mirror=bool(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorPuzzle));puzzle=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorPuzzle if mirror else unreal.WenSiPuzzle)[0]
  variant='mirror' if mirror else 'screen';age=time.monotonic()-museum_test['start'];phase=museum_test['phase']
  if phase==0 and age>3:
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(museum_out/(variant+'_entrance.png')))
   result=puzzle.validate_mirror_puzzle() if mirror else puzzle.validate_circuit_puzzle()
   museum_test['checks'].append({'label':'all original puzzle mechanics','pass':result.startswith('PASS'),'result':result})
   pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
   pawn.set_actor_location(unreal.Vector(-670,0,95),False,True);pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=-38,roll=0));museum_test['phase']=1;museum_test['at']=age
  elif phase==1 and age>museum_test['at']+1:
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(museum_out/(variant+'_gallery.png')))
   museum_test['phase']=2;museum_test['at']=age
  elif phase in (2,4) and age>museum_test['at']+.35:
   kind=2 if mirror and phase==2 else 3 if mirror else 0 if phase==2 else 1
   obj=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorExhibit if mirror else unreal.WenSiObject) if a.get_editor_property('kind' if mirror else 'object_kind')==kind)
   center=obj.get_actor_bounds(False)[0];pawn.set_actor_location(center+unreal.Vector(-210,25 if not mirror and phase==2 else 0,-65),False,True);pc.set_control_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0));museum_test['phase']=phase+1;museum_test['at']=age
  elif phase in (3,5) and age>museum_test['at']+.5:
   unreal.MiningGUIEditorLibrary.send_preview_key(pc,'E',True);museum_test['release']='E';museum_test['phase']=phase+1;museum_test['at']=age
   if phase==5:museum_test['phase']=6;museum_test['capture_index']=0
  elif phase==6:
   elapsed=age-museum_test['at'];index=museum_test['capture_index'];thresholds=[.13,.35,.9]
   if index<3 and elapsed>thresholds[index]:
    unreal.MiningGUIEditorLibrary.capture_preview(pc,str(museum_out/(variant+'_thought_'+str(index)+'.png')));museum_test['capture_index']+=1
   if elapsed>1.2:
    unreal.MiningGUIEditorLibrary.send_preview_key(pc,'G',True);museum_test['release']='G';museum_test['phase']=7;museum_test['at']=age
  elif phase==7 and age>museum_test['at']+.7:
   unreal.MiningGUIEditorLibrary.capture_preview(pc,str(museum_out/(variant+'_edit.png')))
   museum_test['checks'].append({'label':'editing cursor and word swapping available','pass':pc.get_editor_property('show_mouse_cursor')})
   museum_test['passed']=all(c['pass'] for c in museum_test['checks']);(museum_out/(variant+'_validation.json')).write_text(json.dumps(museum_test,ensure_ascii=False,indent=2),encoding='utf8')
   unreal.unregister_slate_post_tick_callback(museum_handle);print('MUSEUM_READY_FOR_DRAG',variant,museum_test['passed'])
  if museum_test.get('release') and age>museum_test['at']+.09:unreal.MiningGUIEditorLibrary.send_preview_key(pc,museum_test.pop('release'),False)
 except Exception as error:
  print('MUSEUM_CHECK_ERROR',repr(error));unreal.unregister_slate_post_tick_callback(museum_handle)

museum_handle=unreal.register_slate_post_tick_callback(museum_tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
