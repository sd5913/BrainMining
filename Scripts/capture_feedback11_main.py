import unreal,json,time,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback11/Main'
OUT.mkdir(parents=True,exist_ok=True)
H=unreal.MiningGUIEditorLibrary
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};r={}
def pose(p,t):
 pawn=s['pc'].get_controlled_pawn();m=pawn.get_component_by_class(unreal.CharacterMovementComponent);m.disable_movement()
 offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location()
 pawn.set_actor_location(p-offset,False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p,t));s['pc'].set_view_target_with_blend(pawn,0)
def run():
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 H.start_floating_preview()
 while not E.get_game_world():yield .2
 w=E.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(w,0)
 yield 8
 pc=s['pc'];H.resize_preview_viewport(pc,1280,720)
 opening=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)[0];opening.start_game()
 while not opening.get_editor_property('bFinished'):yield .2
 room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
 pick=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
 target=pick.get_actor_location();pose(target+unreal.Vector(-100,0,70),target);yield .3
 assert room.interact(pc)
 target=opening.get_editor_property('door_title_location');pose(target-unreal.Vector(180,0,0),target);yield .5
 decal=opening.get_components_by_class(unreal.DecalComponent)[0]
 assert abs(decal.get_editor_property('fade_in_duration')-2.4)<.01
 r['title_fade_seconds']=decal.get_editor_property('fade_in_duration')
 assert H.capture_preview(pc,str(OUT/'title_early.png'));yield 2.1
 assert H.capture_preview(pc,str(OUT/'title_full.png'))
 assert H.prepare_collection_preview_area(pc,2,False)
 yield 3
 pose(unreal.Vector(-15160,-12,6560),unreal.Vector(-14750,0,6545));yield 3
 assert H.capture_preview(pc,str(OUT/'office.png'))
 stage=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.PopupCloseStage)[0]
 assert not any(c.get_name()=='OfficeBlastDoorArt' for c in stage.get_components_by_class(unreal.StaticMeshComponent))
 assert next(c for c in stage.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeBlastDoor').is_visible()
 r['rays']=[]
 for y in [-75,-40,0,40,75]:
  h=unreal.SystemLibrary.line_trace_single(w,unreal.Vector(-15160,y,6560),unreal.Vector(-14650,y,6560),unreal.TraceTypeQuery.ECC_VISIBILITY,True,[pc.get_controlled_pawn()],unreal.DrawDebugTrace.NONE,True).to_tuple()
  r['rays'].append(str(h))
 hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGame03')
 assert H.prepare_collection_preview_area(pc,index,False)
 hall.set_actor_tick_enabled(False)
 pose(unreal.Vector(46650,11750,15750),unreal.Vector(47200,11750,15300));yield 3
 assert H.capture_preview(pc,str(OUT/'museum.png'))
 soil=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.GeneratedMiningRock) if a.actor_has_tag('BM_MuseumDownwardSoil'))
 p,e=soil.get_actor_bounds(False);assert abs(p.z+e.z-15320)<1
 r['soil_top']=p.z+e.z
 # Walk on a plain isolated floor using the actual character movement.
 floor=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.StaticMeshActor)[0]
 c=floor.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_collision_profile_name('BlockAll')
 floor.set_actor_hidden_in_game(False);c.set_visibility(True);floor.set_actor_enable_collision(True)
 floor.set_actor_location(unreal.Vector(0,0,49975),False,True);floor.set_actor_rotation(unreal.Rotator(),False);floor.set_actor_scale3d(unreal.Vector(40,18,.5))
 pawn=pc.get_controlled_pawn();pawn.set_actor_location(unreal.Vector(0,0,50100),False,True)
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_WALKING);yield 1
 end=time.monotonic()+2.4
 while time.monotonic()<end:
  pawn.add_movement_input(unreal.Vector(1,0,0),1,True);yield 0
 move.stop_movement_immediately()
 marks=[c for c in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(c) and c.component_has_tag('MiningFootprint')]
 assert len(marks)>=3
 for mark in marks:
  assert mark.get_editor_property('fade_start_delay')==3
  assert abs(mark.get_editor_property('fade_duration')-12)<.01
 r['continuous_fade_footprints']=len(marks)
 target=unreal.Vector(300,0,50000);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target));yield .2
 assert H.capture_preview(pc,str(OUT/'footprints.png'))
 yield 16
 assert not [c for c in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(c) and c.component_has_tag('MiningFootprint')]
 r['passed']=True;finish()
def finish(error=None):
 if error:r['error']=error
 (OUT/'capture.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);L.editor_request_end_play();unreal.SystemLibrary.quit_editor()
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<160
  s['next']=time.monotonic()+next(gen)
 except StopIteration:pass
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
