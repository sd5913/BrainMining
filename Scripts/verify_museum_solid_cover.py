"""Inspect the stage-one solid cover in the actual MainGame museum."""
import unreal,time,pathlib,json,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumSolidCover'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[]};state={'next':0}
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def run():
 while not ed.get_game_world():yield .2
 w=ed.get_game_world()
 def actors(c):return unreal.GameplayStatics.get_all_actors_of_class(w,c)
 while not actors(unreal.MiningExhibitHall):yield .2
 hall=actors(unreal.MiningExhibitHall)[0]
 while not hall.get_editor_property('bReady'):yield .2
 for o in actors(unreal.MiningOpening):o.start_game();o.confirm_new_game(True)
 while any(not o.get_editor_property('bFinished') for o in actors(unreal.MiningOpening)):yield .2
 pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 check(helper.prepare_collection_preview_area(pc,index,False),'MainGame museum active')
 hall.set_actor_tick_enabled(False);yield 2
 puzzle=actors(unreal.WenSiMirrorPuzzle)[0];origin=puzzle.get_actor_location()
 cover=next(a for a in actors(unreal.WenSiMirrorExhibit) if a.get_editor_property('kind')==1)
 parts={p.get_name():p for p in cover.get_components_by_class(unreal.StaticMeshComponent) if p.get_name().startswith('Part')}
 check(sum(bool(p.static_mesh and p.is_visible()) for p in parts.values())==4,'Roof, two sides and continuous back; no bars')
 panel=parts['Part3'];size=panel.get_editor_property('relative_scale3d')*100
 check(abs(size.x-12)<.01 and abs(size.y-228)<.01 and abs(size.z-240)<.01,'Back panel fills the previous railing span')
 check(panel.get_editor_property('render_custom_depth') and panel.get_overlay_material() is None,'Clean white silhouette retained')
 result=puzzle.validate_mirror_puzzle();report['puzzle_checks']=result;check(result.startswith('PASS'),result)
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def pose(eye,target):
  p=origin+unreal.Vector(*eye);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location()
  pawn.set_actor_location(p-offset,False,True);move.stop_movement_immediately()
  pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p,origin+unreal.Vector(*target)))
 for name,eye,target in [('front',(390,-100,190),(650,0,145)),('back',(900,-150,210),(650,0,155))]:
  pose(eye,target);yield 1.5;check(helper.capture_preview(pc,str(out/(name+'.png'))),'Capture solid cover '+name)
 cover.set_editor_property('word',unreal.WenSiMirrorWord.PASSABLE);cover.refresh_art_model()
 check(panel.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN).value==0,'Passable word opens solid panel collision')
 pose((390,-100,190),(650,0,145));yield 1.5
 check(helper.capture_preview(pc,str(out/'passable.png')),'Capture passable cover')
 report['passed']=True
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();yield .5
task=run()
def tick(dt):
 if time.monotonic()<state['next']:return
 try:state['next']=time.monotonic()+next(task)
 except StopIteration:
  (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
  unreal.unregister_slate_post_tick_callback(handle);print('SOLID_COVER_VERIFIED',report['passed'])
 except Exception:
  report['error']=traceback.format_exc();(out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
  unreal.unregister_slate_post_tick_callback(handle);print(report['error'])
  unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
handle=unreal.register_slate_post_tick_callback(tick)
if not ed.get_game_world():helper.start_floating_preview()
