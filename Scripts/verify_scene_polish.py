"""Reload the saved MainGame and inspect the changed rooms in actual PIE."""
import unreal,json,time,traceback,math
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/SceneFeedback'
OUT.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'after':0,'started':time.monotonic()};report={'passed':False,'checks':[],'images':[],'preview_mode':'software Lumen' if '-noraytracing' in unreal.SystemLibrary.get_command_line() else 'project defaults'}
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def finish(error=None):
 report.update(passed=error is None,error=error);(OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 levels.editor_request_end_play();s.update(phase=99,after=time.monotonic()+8)
def pose(tag,p,target):
 index=next(i for i,a in enumerate(s['hall'].areas) if str(a.tag)==tag)
 check(helper.prepare_collection_preview_area(s['pc'],index,tag=='BM_Campaign_MainGameJourney'),'Activate '+tag)
 s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
 # The detached review camera has a different aim from the pawn's held tool.
 # Hide that foreground mesh only in this unsaved preview.
 s['pawn'].set_actor_hidden_in_game(True)
 s['pawn'].set_actor_location(p,False,True);s['camera'].set_actor_location(p,False,True)
 s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(p,target),False)
 s['pc'].set_view_target_with_blend(s['camera'],0)
def shot(name):
 check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Capture '+name);report['images'].append(name+'.png')
def tick(dt):
 if s.get('busy') or time.monotonic()<s['after']:return
 s['busy']=True
 try:
  now=time.monotonic();ph=s['phase']
  if ph==100:
   unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();return
  if ph==99:
   if ed.get_game_world():return
   s.clear();s.update(phase=100,after=now+5);levels.load_level('/Engine/Maps/Entry');return
  check(now-s['started']<400,'Runtime bounded') if now-s['started']>=400 else None
  if ph==0:
   assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(api.get_all_level_actors())
   check(len([a for a in aa if a.get_actor_label().startswith('MAIN_GAME_CrystalMotes_') and a.get_actor_scale3d().x==3])==2,'Original mushroom atmosphere restored')
   check(not any('Heaven spout catch basin' in a.get_actor_label() for a in aa),'Blue clue disk removed')
   check(not any(a.get_actor_label()=='Office_Entry_OpenDoor' for a in aa),'Old overlapping entrance door mesh removed')
   check(len(next(a for a in aa if isinstance(a,unreal.MiningCollectionField)).authored_placements)==11,'All eleven authored collections retained')
   check(len([a for a in aa if a.get_actor_label().startswith('POLISH_Office_')])==3,'Office wall returns reload')
   check(len([a for a in aa if 'Soft window daylight' in a.get_actor_label()])==4,'Original study lighting restored')
   check(not any(a.get_actor_label().startswith('POLISH_Study_') for a in aa),'Experimental study lights removed')
   shell=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell');sc,se=shell.get_actor_bounds(False)
   check(se.x>=2100 and se.y>=2100 and se.z>=2200,'Campaign rocks fit latest heaven room dimensions')
   sky=next(a for a in aa if a.get_actor_label()=='COL_Heaven_InteriorCloudSky')
   check(sky.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Sky collision remains disabled after reload')
   ignored=[a for a in aa if not isinstance(a,(unreal.GeneratedMiningRock,unreal.DynamicMeshActor))]
   center=shell.get_actor_location()
   collar=next(a for a in aa if a.get_actor_label()=='COL_Heaven_SeamCollar')
   check(unreal.MiningDesignerLibrary.get_campaign_rock_source(collar).get_triangle_count()>0,'Heaven entrance seam collar saved')
   water=next(a for a in aa if isinstance(a,unreal.HeavenToiletWater))
   check(water.radius>=2000 and (water.get_actor_location()-center).length()<3,'Water covers the complete centered floor disk')
   check(any(isinstance(a,unreal.HeavenPipeDrip) for a in aa),'Visible falling drops and impact splashes saved')
   for a in aa:
    for c in a.get_components_by_class(unreal.MeshComponent):
     assert not any(m and m.get_name().endswith('_Relief') for m in c.get_materials()),'Unwanted relief still assigned: '+a.get_actor_label()
   check(True,'Original wall materials restored throughout start room and first mine')
   for height in [300,1400,3200]:
    start=center+unreal.Vector(0,0,height)
    for angle in range(0,360,30):
     end=start+unreal.Vector(1900*math.cos(math.radians(angle)),1900*math.sin(math.radians(angle)),0)
     hit=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),start,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True)
     assert hit is None or not hit.to_tuple()[0], 'Old terrain intrudes into resized heaven room: '+str(hit)
   check(True,'36 collision rays confirm diggable and outer-shell heaven clearance')
   frame=water.spill_coordinate_frame
   seam_ignore=[a for a in aa if not isinstance(a,(unreal.DiggableRock,unreal.DynamicMeshActor))]
   for x in [400,600]:
    start=unreal.MathLibrary.transform_location(frame,unreal.Vector(x,0,220))
    for offset in [unreal.Vector(0,650,0),unreal.Vector(0,-650,0),unreal.Vector(0,0,450),unreal.Vector(0,0,-400)]:
     end=start+unreal.MathLibrary.transform_direction(frame,offset)
     hit=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),start,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,seam_ignore,unreal.DrawDebugTrace.NONE,True)
     assert hit is not None and hit.to_tuple()[0], 'Gap at heaven entrance collar: '+str((x,offset))
   check(True,'Eight collision rays confirm solid entrance walls, ceiling and floor')
   minerals=next(a for a in aa if isinstance(a,unreal.MiningTreasureField)).get_editor_property('authored_placements')
   check(any(str(p.treasure_id)=='6' and p.transform.translation.distance(unreal.Vector(35454.425,14201.093,19255.948))<2 for p in minerals),'First red-route dead-end malachite retained')
   check(any(a.get_actor_label()=='POLISH_StageSoil_Guide' for a in aa),'Stage guide light reloads')
   plug=next(a for a in aa if a.get_actor_label()=='MAIN_GAME_Museum_DownwardSoil');center,extent=plug.get_actor_bounds(False)
   check(abs(center.z-extent.z-14800)<2,'Museum soil reaches ceiling underside')
   check(abs(extent.x-220)<2 and abs(extent.y-220)<2,'Museum soil matches the original 440 cm aperture')
   camera=api.spawn_actor_from_class(unreal.CameraActor,unreal.Vector());camera.tags=['ScenePolishReviewCamera']
   unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
   helper.start_floating_preview();s.update(phase=1,after=now+6);return
  w=ed.get_game_world()
  if not w:return
  s['w']=w
  if ph==1:
   hall=actors(unreal.MiningExhibitHall)[0]
   if not hall.ready:return
   for opening in actors(unreal.MiningOpening):
    if not opening.finished:opening.start_game();s['after']=now+1;return
   s.update(hall=hall,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),camera=unreal.GameplayStatics.get_all_actors_with_tag(w,'ScenePolishReviewCamera')[0])
   check(helper.prepare_collection_preview_area(s['pc'],len(hall.areas)-1,False),'Preview unlocked exits through the existing area helper')
   unreal.SystemLibrary.execute_console_command(w,'DisableAllScreenMessages')
   water=actors(unreal.HeavenToiletWater)[0];frame=water.spill_coordinate_frame
   point=lambda p:unreal.MathLibrary.transform_location(frame,unreal.Vector(*p))
   s['toilet']=next(a for a in actors(unreal.HeavenToilet) if a.get_actor_location().z>0)
   s['views']=[
    ('start_room_walls','BM_Campaign_MainGame01',unreal.Vector(-34605,55,11270),unreal.Vector(-34262,63,11240)),
    ('office_entry','BM_Campaign_MainGameOffice',unreal.Vector(-16860,0,6570),unreal.Vector(-16600,-100,6570)),
    ('office_partition','BM_Campaign_MainGameOffice',unreal.Vector(-16320,20,6570),unreal.Vector(-16462,166,6580)),
    ('mushroom_dream_dust','BM_Campaign_MainGame02',unreal.Vector(-11000,-320,7000),unreal.Vector(-10300,0,7600)),
    ('stage_exit_guide','BM_Campaign_MainGameJourney',unreal.Vector(32050,11750,20100),unreal.Vector(33300,11750,20120)),
    ('heaven_entrance','BM_Campaign_MainGame03',point((1200,0,190)),point((2780,0,230))),
    ('heaven_latest','BM_Campaign_MainGame03',point((2150,-700,360)),point((2580,0,200))),
    ('heaven_drip','BM_Campaign_MainGame03',point((-1590,-70,230)),point((-1400,125,184))),
    ('museum_ceiling','BM_Campaign_MainGameMuseum',unreal.Vector(48000,11800,14530),unreal.Vector(47200,11750,14810)),
    ('night_study','BM_Campaign_MainGameJourney',unreal.Vector(8000,-1800,14600),unreal.Vector(15000,3600,14000))]
   stage=actors(unreal.CardRoomStage)[0]
   marks=[c for c in stage.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('CardMark')]
   check(len(marks)==17 and all(c.static_mesh==unreal.load_asset('/Engine/BasicShapes/Plane') for c in marks),'All seventeen physical cards have artwork planes')
   s.update(phase=2,index=0);return
  if ph==2:
   name,tag,p,target=s['views'][s['index']];pose(tag,p,target);s.update(phase=3,after=now+(9 if name=='heaven_latest' else 3));return
  if ph==3:
   name=s['views'][s['index']][0];shot(name)
   if name=='heaven_drip':
    drip=actors(unreal.HeavenPipeDrip)[0]
    check(drip.droplets.get_instance_count()==22 and drip.splashes.get_instance_count()==18,'Visible pipe flow uses bounded falling-drop and splash pools')
   if name=='heaven_latest':
    toilet=s['toilet'];check(s['pawn'].get_actor_location().distance(toilet.water_outlet.get_world_location())>toilet.close_distance,'Fountain observed outside proximity radius')
    check(abs(toilet.duck_scale-.54)<.001,'Ducks are three times the original linear size')
    check(toilet.water_running and toilet.water_jet.is_active() and toilet.lid_open_amount>.99,'Water stays active far from player')
    ducks=[c for c in toilet.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh==toilet.spray_duck.static_mesh and c.is_visible()]
    check(len(ducks)>=2,'Repeated fountain bursts emit multiple ducks');s['duck_positions']={c.get_name():c.get_world_location() for c in ducks}
    s.update(phase=4,after=now+1,duck_deadline=now+40);return
   s['index']+=1
   if s['index']==len(s['views']):finish()
   else:s['phase']=2
  if ph==4:
   toilet=s['toilet'];resting=[c for c in toilet.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() in s['duck_positions'] and c.is_visible() and c.get_world_location().distance(s['duck_positions'][c.get_name()])<1]
   resting=[c for c in resting if c.get_world_location().z<toilet.get_actor_location().z+45 and c.get_world_location().distance(unreal.Vector(toilet.get_actor_location().x,toilet.get_actor_location().y,c.get_world_location().z))>250]
   if not resting and now<s['duck_deadline']:
    s['duck_positions']={c.get_name():c.get_world_location() for c in toilet.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh==toilet.spray_duck.static_mesh and c.is_visible()}
    s['after']=now+1;return
   check(len(resting)>=1,'Emitted ducks land on the floor beyond the toilet dais')
   check(any(a.get_world_location().distance(b.get_world_location())>100 for a in resting for b in resting) or len(resting)==1,'Ducks use distinct landing positions')
   report['resting_ducks']=len(resting);shot('heaven_ducks_landed')
   p=resting[0].get_world_location();s['camera'].set_actor_location(p+unreal.Vector(-45,40,28),False,True)
   s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(s['camera'].get_actor_location(),p+unreal.Vector(0,0,4)),False)
   s.update(phase=5,after=now+1)
  if ph==5:
   shot('rubber_duck_closeup');s['index']+=1;s['phase']=2
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
