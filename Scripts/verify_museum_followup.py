"""Verify the saved streamed art, native runtime walls and real balance motion."""
import unreal,time,json,pathlib,traceback
out=pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
state={'next':0};report={'passed':False,'checks':[]}
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def allof(cls):return unreal.GameplayStatics.get_all_actors_of_class(state['world'],cls)
def tag(a,t):return t in [str(v) for v in a.tags]
def run():
 while not ed.get_game_world():yield .2
 state['world']=ed.get_game_world();w=state['world'];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
 while not allof(unreal.MiningExhibitHall):yield .2
 hall=allof(unreal.MiningExhibitHall)[0]
 while not hall.get_editor_property('bReady'):yield .2
 for o in allof(unreal.MiningOpening):o.start_game();o.confirm_new_game(True)
 while any(not o.get_editor_property('bFinished') for o in allof(unreal.MiningOpening)):yield .2
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameMuseum')
 check(helper.prepare_collection_preview_area(pc,index,False),'MainGame museum active');hall.set_actor_tick_enabled(False);yield 3
 puzzle=allof(unreal.WenSiMirrorPuzzle)[0];origin=puzzle.get_actor_location();ex={a.get_editor_property('kind'):a for a in allof(unreal.WenSiMirrorExhibit)}
 check(len(ex)==12,'Twelve original gameplay exhibits retained')
 actors=allof(unreal.Actor);new=[a for a in actors if tag(a,'MuseumRefreshFollowup')]
 check(len(new)==11,'Six plinth visuals, four balance visuals and shark loaded from saved sublevel')
 for k,n in [(5,'SM_FoldingScreen'),(8,'SM_ConductiveBlock')]:
  c=ex[k].get_editor_property('art_visual');check(c.static_mesh.get_name()==n and c.is_visible(),'Replacement rendered: '+n)
  check(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Original thinking/collision proxy retained: '+n)
  if k==5:
   bounds=unreal.SystemLibrary.get_component_bounds(c)
   check(bounds[1].z>115 and bounds[1].y>115 and bounds[1].x<17,'Screen is upright and spans the original doorway')
   check(c.get_overlay_material() is None,'Screen finish is not obscured by opaque shell outline')
 check(ex[8].get_editor_property('art_visual').get_material(0).get_name()=='MI_MuseumMetal032','Conductive block uses PBR metal')
 walls=[];enamel=[];plinths=[]
 for a in actors:
  label=a.get_actor_label()
  if label.startswith(('RewardSide_','RewardFront_','RewardHeader_')):
   check(a.static_mesh_component.get_material(0).get_name()=='MI_Stucco01','Endpoint outer wall finish '+label);walls.append(label)
  if label.startswith(('HopperLeg_','HopperSlope_','CatchTray_','ContactInsulator_','BallContactPedestal')):
   check(a.static_mesh_component.get_material(0).get_name()=='MI_ApparatusEnamel','Nonmetal housing '+label);enamel.append(label)
  if label.startswith('MM_') and 'MuseumStand' in label:
   check(a.static_mesh_component.static_mesh.get_name()=='SM_PillarStand02','Existing museum plinth '+label);plinths.append(label)
 check(len(walls)==5 and len(plinths)==6 and len(enamel)==12,'Expected outer wall, housing and plinth counts')
 portal=allof(unreal.MouseMuseumPortal)[0]
 for c in portal.get_components_by_class(unreal.StaticMeshComponent):
  if c.get_name() in ['GalleryBack','GalleryLeftWall','GalleryRightWall']:check(c.get_material(0).get_name()=='MI_Stucco01','Runtime generated wall '+c.get_name())
 check(not any(a.get_actor_label().startswith('StatuePlinth_') for a in actors),'Empty first-room plinth deleted')
 shark=next(a for a in new if 'BronzeShark' in a.get_actor_label());check(shark.static_mesh_component.static_mesh.get_name()=='SM_BronzeShark','Bronze shark displayed on its own base')
 # Record transforms before validation then after an actual Heavy assignment.
 beam=next(a for a in actors if tag(a,'WenSiBalanceBeam'));left=next(a for a in actors if tag(a,'WenSiBalanceLeftTray'));right=next(a for a in actors if tag(a,'WenSiBalanceRightTray'))
 children=[a for a in new if a.get_attach_parent_actor() in (beam,left,right)]
 check(len(children)==3,'Imported beam and pans attached to original animated carriers')
 result=puzzle.validate_mirror_puzzle();report['puzzle_validation']=result;check(result.startswith('PASS'),result)
 yield 1
 report['ordinary']={'pitch':beam.get_actor_rotation().pitch,'leftZ':left.get_actor_location().z-origin.z,'rightZ':right.get_actor_location().z-origin.z}
 move=pawn.get_component_by_class(unreal.CharacterMovementComponent);move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
 def pose(eye,target):
  offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(origin+unreal.Vector(*eye)-offset,False,True);move.stop_movement_immediately();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(origin+unreal.Vector(*eye),origin+unreal.Vector(*target)))
 for name,eye,target in [('shark',(-440,150,150),(-230,360,60)),('screen',(1220,220,160),(970,0,120)),('balance_ordinary',(1390,-200,140),(1590,-430,100)),('apparatus',(1700,-70,225),(1900,120,130)),('metal_block',(1300,320,160),(1340,320,115)),('endpoint_walls',(2270,-350,120),(2432,-350,120))]:
  pose(eye,target);yield 2
  check(helper.capture_preview(pc,str(out/(name+'.png'))),'Capture '+name)
 ex[6].set_editor_property('word',unreal.WenSiMirrorWord.HEAVY);ex[6].refresh_art_model();yield 2
 report['heavy']={'pitch':beam.get_actor_rotation().pitch,'leftZ':left.get_actor_location().z-origin.z,'rightZ':right.get_actor_location().z-origin.z}
 check(beam.get_actor_rotation().pitch>12 and left.get_actor_location().z-origin.z<22 and right.get_actor_location().z-origin.z>65,'Heavy sculpture tips balance: left down, right up')
 for a in children:
  parent=a.get_attach_parent_actor()
  check(abs(a.get_actor_location().z-parent.get_actor_location().z)<.1,'Imported moving art follows '+parent.get_actor_label())
 pose((1390,-200,140),(1590,-430,100));yield 2;check(helper.capture_preview(pc,str(out/'balance_heavy.png')),'Capture tilted balance')
 report['passed']=True
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play();yield .5
 print('FOLLOWUP_VERIFIED',report['passed'])
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
