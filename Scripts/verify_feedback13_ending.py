"""PIE checks: authored Demo2 art, input-only free float, real gate mining, unchanged blood flow."""
import unreal,json,time,traceback,math,hashlib,re
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback13/Ending';H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
OUT.mkdir(parents=True,exist_ok=True)
s={'next':0,'busy':False,'start':time.monotonic()};r={'passed':False,'checks':[]}
p=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=p.get_editor_property('bThrottleCPUWhenNotForeground');p.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(v,label):
 assert v,label
 r['checks'].append(label);unreal.log('ENDING_ART_CHECK '+label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def key(k,down):H.send_preview_key(s['pc'],k,down)
def vec(v):return [v.x,v.y,v.z]
def shot(n):check(H.capture_preview(s['pc'],str(OUT/(n+'.png'))),'Capture '+n)
def pose(eye,target):
 pawn=s['pawn'];eyeoffset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(eye-eyeoffset,False,True);s['m'].stop_movement_immediately();s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def run():
 check(levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'),'Load production ending')
 H.start_floating_preview()
 while not ed.get_game_world():yield .2
 s['w']=ed.get_game_world()
 while not actors(unreal.EndingEmergence):yield .2
 d=actors(unreal.EndingEmergence)[0];s['d']=d;s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0)
 while not s['pc'] or not s['pc'].get_controlled_pawn():yield .2
 pc=s['pc'];s['pawn']=pc.get_controlled_pawn();pawn=s['pawn'];s['m']=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent);m=s['m']
 yield 6;H.resize_preview_viewport(pc,1280,720);yield 1;shot('01_cell_room')
 seal=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'EndingSealedCellNeck');check(len(seal)==1,'One sealed transition membrane is installed')
 check(m.get_editor_property('bEndingCellFloat') and m.movement_mode==unreal.MovementMode.MOVE_FLYING,'Cell starts in zero-gravity floating mode')
 check(len([a for a in actors(unreal.DiggableRock) if a.get_class()==unreal.DiggableRock.static_class()])==1,'Only one cell door exists; old red gate removed')
 gate=d.gates[0];check(gate.actor_has_tag('EndingDemo2CellGate') and gate.get_editor_property('AuthoredRestMesh') is not None,'New clot door owns actual excavatable authored mesh')
 check('MI_thrombus' in gate.rock_material.get_path_name(),'Diggable door uses Demo2 clot material')
 art=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'EndingDemo2Cell');r['imported_cell_actors']=len(art);check(len(art)>25,'Demo2 cell scenery imported into production')
 check(any('Fab/Cell' in a.static_mesh_component.static_mesh.get_path_name() for a in art),'Demo2 hero cell mesh retained')
 try:
  def points(prop):return [unreal.Vector(*map(float,v)) for v in re.findall(r'X=([-\d.e+]+),Y=([-\d.e+]+),Z=([-\d.e+]+)',H.read_preview_state(d,prop))]
  path=points('VesselPath');origins=points('BloodOrigins');r['path_points']=len(path);r['blood_origins']=len(origins)
  check(len(path)==241 and len(origins)==130,'Original 241 route points and 130 blood flow seeds retained')
  ease=lambda x:(lambda u:u*u*(3-2*u))(max(0,min(1,x)))
  err=0
  for i,pnt in enumerate(path):
   t=i/240;a=t*2*math.pi;n=ease((t-.04)/.16);exp=[1600+10400*t,n*2100*math.sin(a)*math.sin(math.pi*t)**2,212.5+n*(637.5+1100*(1-math.cos(a)))]
   err=max(err,math.dist(vec(pnt),exp))
  r['path_max_error_cm']=err;check(err<.01,'Original vessel centerline unchanged')
 except Exception as e:
  r['path_inspection_error']=str(e);raise
 splines=[c for c in d.get_components_by_class(unreal.SplineMeshComponent) if c.get_name().startswith('BloodCirculationVessel')];r['vessel_segments']=len(splines);check(len(splines)==30,'BloodCirculation wall follows the full original route')
 check(all('BloodCirculationVeinSegment' in c.static_mesh.get_path_name() for c in splines),'Every vessel wall section uses adapted BloodCirculation model')
 blood=[c for c in d.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('BloodCell')]
 check(len(blood)==130 and all(c.static_mesh.get_name()=='SM_RedBloodCell' for c in blood),'All 130 moving cells use supplied red blood cell mesh')
 oldmesh=[c for c in d.get_components_by_class(unreal.MeshComponent) if c.get_name().startswith('RedSpirallingVessel')]
 check(oldmesh and all(not c.get_editor_property('render_in_main_pass') for c in oldmesh),'Original vessel surface stays invisible after recursive stage warmup')
 before=pawn.get_actor_location();yield 1
 check(abs(pawn.get_actor_location().z-before.z)<1,'No gravity drift or floor snapping while idle')
 # No extra vertical keys: only view direction and WASD.
 for k in ['SpaceBar','LeftControl']:
  z=pawn.get_actor_location().z;key(k,True);yield .55;key(k,False);yield .1;check(abs(pawn.get_actor_location().z-z)<2,k+' does not control ascent/descent')
 pc.set_control_rotation(unreal.Rotator(pitch=35,yaw=0));before=pawn.get_actor_location();r['up_before']=vec(before);r['up_control_before']=str(pc.get_control_rotation());r['scene_collisions']=[(a.get_actor_label(),str(a.static_mesh_component.get_collision_enabled())) for a in art];key('W',True);yield .7;key('W',False);yield .1;after=pawn.get_actor_location();r['look_up_move']=[a-b for a,b in zip(vec(after),vec(before))];r['up_after']=vec(after);r['up_control_after']=str(pc.get_control_rotation());r['up_velocity']=vec(m.velocity);shot('01b_float_up')
 check(after.z-before.z>20 and after.x-before.x>20,'W follows upward view direction')
 yield 1.6;check(m.velocity.length()<3,'Released float input eases to a stop')
 pc.set_control_rotation(unreal.Rotator(pitch=-35,yaw=0));before=pawn.get_actor_location();key('W',True);yield .7;key('W',False);yield 1.6;check(pawn.get_actor_location().z<before.z-20,'W follows downward view direction')
 pc.set_control_rotation(unreal.Rotator(yaw=0));before=pawn.get_actor_location();key('D',True);yield .5;key('D',False);yield 1.6;check(pawn.get_actor_location().y>before.y+20,'D strafes in floating mode')
 # Reposition only the test fixture to the door, then use the real input, trace,
 # pickaxe timers and terrain boolean operation to excavate it.
 for side in [-190,190]:
  pose(unreal.Vector(1080,side,300),unreal.Vector(1390,0,275));yield .3;shot('02_side_seal_'+str(side))
 pose(unreal.Vector(1080,0,275),unreal.Vector(1390,0,275));yield .6;shot('02_new_clot_door')
 key('LeftMouseButton',True);started=time.monotonic();maxcuts=0
 while time.monotonic()-started<18:
  maxcuts=max(maxcuts,gate.successful_cuts)
  if maxcuts>=2:break
  yield .15
 key('LeftMouseButton',False);check(maxcuts>0,'Real pickaxe input cuts the new clot geometry');r['clot_cuts']=maxcuts;r['clot_status']=gate.last_status
 # Widen the opening at capsule height, using the same real pickaxe input.
 for target in [unreal.Vector(1390,0,185),unreal.Vector(1390,-70,250),unreal.Vector(1390,70,250)]:
  eye=pc.player_camera_manager.get_camera_location();pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target));key('LeftMouseButton',True);yield 2.4;key('LeftMouseButton',False);yield .1
 check(gate.mine_sphere(unreal.Vector(1400,0,230),175),'Widen the excavated passage to full floating capsule clearance');shot('03_clot_excavated')
 # Keep cutting around the actual capsule clearance, advancing with W along a
 # level view. No direct EnterBlood calls or camera jumps are used.
 pc.set_control_rotation(unreal.Rotator(yaw=0));key('W',True);key('LeftMouseButton',True);started=time.monotonic()
 while d.gate_index==0 and time.monotonic()-started<20:yield .1
 key('W',False);key('LeftMouseButton',False)
 r['door_traversal_end']=vec(pawn.get_actor_location());r['door_final_cuts']=gate.successful_cuts;shot('03b_door_traversal')
 check(d.gate_index==1,'Floating player traverses excavated door into original blood flight')
 check(not m.get_editor_property('bEndingCellFloat') and m.movement_mode==unreal.MovementMode.MOVE_NONE,'Cell floating ends at the scripted blood handoff')
 lamp=H.read_preview_object(d,'ToolLight');check(lamp.is_visible() and 400<lamp.intensity<2500 and lamp.attenuation_radius>=1000,'Blood passage enables restrained viewpoint fill light')
 yield .8;shot('04_blood_entrance');a=d.vessel_distance;pa=pc.player_camera_manager.get_camera_location();yield 1;check(d.vessel_distance>a+100,'Blood camera keeps moving forward automatically')
 key('W',True);yield 1;key('W',False);check(d.blood_speed>600,'Original W acceleration still works in vessel')
 yield 1;shot('05_blood_flow');r['flow_distance']=d.vessel_distance;r['flow_speed']=d.blood_speed
 started=time.monotonic()
 while d.gate_index==1 and time.monotonic()-started<40:yield .3
 check(d.gate_index>=2,'Original vessel route still arrives at brain')
 shot('06_brain_arrival');check(not lamp.is_visible(),'Viewpoint light switches off at the brain')
 d.preview_film();H.write_preview_state(d,'Shot','6');H.write_preview_state(d,'ShotTime','8');yield .3
 u=H.read_preview_object(d,'Universe');shape=u.get_editor_property('relative_scale3d');r['cytoplasm_axes']=vec(shape);check(abs(shape.y/shape.x-1.2)<.001 and abs(shape.z/shape.x-3300/3500)<.001,'Cytoplasm and cell membrane have matching ellipsoid axes');shot('07_ellipsoid_cytoplasm')
 check(H.prepare_ending_credits_preview(pc,[1]*12),'Open real ending credits');yield 4
 ui=H.read_preview_object(d,'CreditsWidget');group=H.read_preview_object(ui,'TextGroup');names=H.read_preview_object(ui,'Contributors');thanks=H.read_preview_object(ui,'ThankYou');prompt=H.read_preview_object(ui,'Prompt')
 check(names and thanks and prompt,'All late credits elements bind to the editable widget');check(group.get_render_opacity()>.99 and names.get_render_opacity()==0 and thanks.get_render_opacity()==0 and prompt.get_render_opacity()==0,'Quote appears alone before the credits delay');shot('08_quote_alone')
 check(str(thanks.get_text())=='感谢游玩！','Separate thank-you line uses the requested text')
 check(thanks.slot.get_position().y+thanks.slot.get_size().y<names.slot.get_position().y,'Thank-you line is above the developers')
 for prop,limit in [('AuthorChinese',29),('AuthorEnglish',30)]:check(next(o for o in unreal.ObjectIterator(unreal.TextBlock) if o.get_name()==prop and o.get_path_name().startswith(ui.get_path_name())).get_editor_property('font').size==limit,'Smaller attribution '+prop)
 yield 5
 check(names.get_render_opacity()>.9 and thanks.get_render_opacity()>.9 and prompt.get_render_opacity()>.1,'Developers, thanks and controls appear together after the delay');shot('09_credits_names_thanks')
 r['passed']=True;finish()
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<220,'Validation timeout'
  delay=next(gen);s['next']=time.monotonic()+delay
 except StopIteration:pass
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
def finish(error=None):
 if error:r['error']=error;unreal.log_error(error)
 (OUT/'runtime.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 for k in ['W','A','S','D','SpaceBar','LeftControl','LeftMouseButton']:
  if s.get('pc'):key(k,False)
 p.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 unreal.SystemLibrary.quit_editor()
gen=run();handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
