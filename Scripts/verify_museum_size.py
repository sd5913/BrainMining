"""Run size regression checks in the MainGame-streamed museum and capture art."""
import unreal,time,json,pathlib,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumSize'
out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
state={'next':0};report={'passed':False,'samples':[]}
def run():
 while not ed.get_game_world():yield .2
 w=ed.get_game_world()
 def allof(c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
 while not allof(unreal.MiningExhibitHall):yield .2
 hall=allof(unreal.MiningExhibitHall)[0]
 while not hall.get_editor_property('bReady'):yield .2
 for o in allof(unreal.MiningOpening):o.start_game();o.confirm_new_game(True)
 while any(not o.get_editor_property('bFinished') for o in allof(unreal.MiningOpening)):yield .2
 pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 idx=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 assert helper.prepare_collection_preview_area(pc,idx,False);hall.set_actor_tick_enabled(False);yield 2
 puzzle=allof(unreal.WenSiMirrorPuzzle)[0];origin=puzzle.get_actor_location()
 report['runtime_checks']=puzzle.validate_mirror_puzzle()
 assert report['runtime_checks'].startswith('PASS'),report['runtime_checks']
 exhibits={a.get_editor_property('kind'):a for a in allof(unreal.WenSiMirrorExhibit)}
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def bounds(a):
  c=a.get_editor_property('art_visual');p,e,r=unreal.SystemLibrary.get_component_bounds(c);return {'bottom':p.z-e.z-origin.z,'top':p.z+e.z-origin.z,'center':a.get_actor_location().z-origin.z}
 for kind,eye,target in [(6,(1400,-230,135),(1500,-430,100)),(8,(1280,260,190),(1340,320,130)),(12,(1000,320,180),(1110,480,140))]:
  a=exhibits[kind]
  for word in [unreal.WenSiMirrorWord.BIG,unreal.WenSiMirrorWord.SMALL,unreal.WenSiMirrorWord.ORDINARY]:
   a.set_editor_property('word',word);a.refresh_art_model();before=bounds(a);yield 1.5;after=bounds(a)
   report['samples'].append({'kind':kind,'word':str(word),'immediate':before,'settled':after})
   if kind in (8,12):assert abs(after['bottom']-before['bottom'])<.1,report['samples'][-1]
   p=origin+unreal.Vector(*eye);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(p-offset,False,True);move.stop_movement_immediately();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p,origin+unreal.Vector(*target)));yield .6
   assert helper.capture_preview(pc,str(out/(str(kind)+'_'+word.name+'.png')))
 report['passed']=True
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();yield .5
 print('MUSEUM_SIZE_VERIFIED',report['runtime_checks'])
task=run()
def tick(dt):
 if time.monotonic()<state['next']:return
 try:state['next']=time.monotonic()+next(task)
 except StopIteration:
  (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle)
 except Exception:
  report['error']=traceback.format_exc();(out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);print(report['error'])
handle=unreal.register_slate_post_tick_callback(tick)
if not ed.get_game_world():helper.start_floating_preview()
