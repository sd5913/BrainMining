"""Exercise the museum replacement in MainGame PIE and capture the actual art."""
import unreal,time,json,pathlib,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
state={'start':time.monotonic(),'next':0};report={'passed':False,'checks':[]}
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def allof(cls):return unreal.GameplayStatics.get_all_actors_of_class(state['world'],cls)
def parts(a):return sorted([c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('Part')],key=lambda c:int(c.get_name()[4:]))
def run():
 while not ed.get_game_world():yield .2
 state['world']=ed.get_game_world();w=state['world'];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 while not allof(unreal.MiningExhibitHall):yield .2
 hall=allof(unreal.MiningExhibitHall)[0]
 while not hall.get_editor_property('bReady'):yield .2
 openings=allof(unreal.MiningOpening)
 for opening in openings:
  opening.start_game();opening.confirm_new_game(True)
 while any(not o.get_editor_property('bFinished') for o in openings):yield .2
 yield .5
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 check(helper.prepare_collection_preview_area(pc,index,False),'MainGame museum area activated')
 hall.set_actor_tick_enabled(False);yield 2
 puzzle=allof(unreal.WenSiMirrorPuzzle)[0]
 exhibits={a.get_editor_property('kind'):a for a in allof(unreal.WenSiMirrorExhibit)}
 check(len(exhibits)==12,'All twelve original gameplay exhibits retained')
 for k,mesh in [(0,'SM_Rock'),(3,'SM_Balloon_UE'),(6,'SM_PorcelainHorse'),(12,'SM_Cat')]:
  a=exhibits[k];visual=a.get_editor_property('art_visual')
  check(visual.static_mesh.get_name()==mesh and visual.is_visible(),'Rendered replacement '+mesh)
  check(visual.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Art has no extra collision '+mesh)
 check(exhibits[6].get_editor_property('display_noun')=='雕塑','Scale thought noun is 雕塑')
 smoke=next(c for c in exhibits[4].get_components_by_class(unreal.NiagaraComponent) if c.get_name()=='MuseumSmoke')
 check(smoke.get_asset().get_name()=='NS_MuseumSmoke' and smoke.is_active(),'Epic smoke plume active')
 check(all(not p.get_editor_property('render_in_main_pass') for p in parts(exhibits[4]) if p.static_mesh),'Old smoke spheres hidden')
 for k in (1,7):
  mat=parts(exhibits[k])[0].get_material(0)
  check(mat.get_editor_property('parent').get_name()=='MI_MuseumMetal032','Metal PBR cover '+str(k))
 mat=parts(exhibits[9])[0].get_material(0)
 check(mat.get_editor_property('parent').get_name()=='MI_MuseumPorcelain','White ceramic sphere')
 result=puzzle.validate_mirror_puzzle();report['puzzle_validation']=result
 check(result.startswith('PASS'),result)
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def pose(eye,target):
  offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location()
  pawn.set_actor_location(eye-offset,False,True);move.stop_movement_immediately();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
 for k,name,eye in [(3,'balloon',(-560,-150,240)),(4,'smoke',(0,-180,145)),(12,'cat',(1090,290,135)),(6,'porcelain_horse',(1370,-270,150)),(0,'stone_cover',(420,-45,160)),(9,'ceramic_sphere',(1770,20,215))]:
  a=exhibits[k];center=a.get_actor_location();pose(puzzle.get_actor_location()+unreal.Vector(*eye),center)
  yield 1.5
  check(helper.capture_preview(pc,str(out/(name+'.png'))),'Capture '+name)
 report['passed']=True
 report['exhibits']=[{'kind':k,'label':a.get_actor_label(),'model':str(a.get_editor_property('art_model'))} for k,a in exhibits.items()]
 yield .1
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
 print('MUSEUM_REFRESH_VERIFIED',report['passed'])
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
