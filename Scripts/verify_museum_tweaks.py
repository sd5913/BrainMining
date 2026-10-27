"""Check the updated presentation and real failed-swap feedback in MainGame."""
import unreal,pathlib,time,json,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumTweaks'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
state={'next':0};report={'passed':False,'checks':[]}
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
 idx=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 check(helper.prepare_collection_preview_area(pc,idx,False),'MainGame streamed museum active');hall.set_actor_tick_enabled(False);yield 2
 puzzle=actors(unreal.WenSiMirrorPuzzle)[0];origin=puzzle.get_actor_location();ex={a.get_editor_property('kind'):a for a in actors(unreal.WenSiMirrorExhibit)}
 report['orientations']=[{'kind':k,'actor_rotation':ex[k].get_actor_rotation().to_tuple(),'art_rotation':ex[k].get_editor_property('art_rotation').to_tuple(),'component_rotation':ex[k].get_editor_property('art_visual').get_editor_property('relative_rotation').to_tuple()} for k in (6,12)]
 for k in (1,7):
  p=next(c for c in ex[k].get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='Part0')
  check(p.get_material(0).get_editor_property('parent').get_name()=='MI_MuseumRoughGold','Cover '+str(k)+' uses shared rough gold')
 check(abs(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(unreal.load_asset('/Game/WenSi/Museum/Materials/MI_MuseumRoughGold'),'Roughness')-.48)<.001,'Gold roughness retained')
 props={a.get_actor_label():a for a in actors(unreal.StaticMeshActor)}
 for part in ('Stand','Beam','LeftPan','RightPan'):
  a=props['MM_MedievalScale'+part];check(a.static_mesh_component.static_mesh.get_name()=='SM_MedievalScale'+part,'Approved medieval scale '+part+' loaded')
 check(props['MM_MuseumMiningWeight'].get_attach_parent_actor().actor_has_tag('WenSiBalanceCounterweight'),'Scanned weight follows animated counterweight')
 result=puzzle.validate_mirror_puzzle();report['runtime_checks']=result;check(result.startswith('PASS'),result)
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def pose(eye,target):
  p=origin+unreal.Vector(*eye);offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(p-offset,False,True);move.stop_movement_immediately();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p,origin+unreal.Vector(*target)))
 pose((1720,-90,200),(1900,120,235));yield .8
 check(helper.capture_preview(pc,str(out/'no_space.png')),'Capture actual rejected-swap subtitle')
 for name,eye,target in [('gold_stand',(1720,-90,220),(1900,120,175)),('gold_cover1',(390,-100,190),(650,0,145)),('gold_cover2',(1130,170,170),(1340,320,120)),('horse_front',(1280,-240,140),(1500,-430,105)),('cat_front',(1220,210,150),(1110,480,110)),('balance',(1540,-130,180),(1590,-430,110))]:
  pose(eye,target);yield 1.5;check(helper.capture_preview(pc,str(out/(name+'.png'))),'Capture '+name)
 ex[6].set_editor_property('word',unreal.WenSiMirrorWord.HEAVY);ex[6].refresh_art_model();yield 2
 beam=next(a for a in actors(unreal.StaticMeshActor) if a.actor_has_tag('WenSiBalanceBeam'))
 check(beam.get_actor_rotation().pitch>12,'New scale tilts under heavy sculpture')
 check(abs(props['MM_MedievalScaleLeftPan'].get_actor_rotation().pitch)<.01,'Hanging pan stays horizontal')
 pose((1540,-130,180),(1590,-430,110));yield 1
 check(helper.capture_preview(pc,str(out/'balance_heavy.png')),'Capture tilted balance')
 report['passed']=True
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();yield .5
task=run()
def tick(dt):
 if time.monotonic()<state['next']:return
 try:state['next']=time.monotonic()+next(task)
 except StopIteration:
  (out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);print('MUSEUM_TWEAKS_VERIFIED',report['passed'])
 except Exception:
  report['error']=traceback.format_exc();(out/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');unreal.unregister_slate_post_tick_callback(handle);print(report['error']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
handle=unreal.register_slate_post_tick_callback(tick)
if not ed.get_game_world():helper.start_floating_preview()
