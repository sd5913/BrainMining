"""Render the live MainGame enclosure; native escape tests cover the full journey.

The escaped-state setup is only a camera fixture, not a substitute for the
FullCardAndTelevisionEscape regression and its capsule sweeps.
"""
import json,time,traceback
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir())/'Saved/Validation/StageBoundary'
out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'next':0,'start':time.monotonic()}
report={'passed':False,'screenshots':[],'world_sweeps':[]}

def pose(eye,target):
 p=s['pawn'];v=p.get_actor_eyes_view_point()[0]-p.get_actor_location()
 p.set_actor_location(unreal.Vector(*eye)-v,False,True)
 s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye),unreal.Vector(*target)))

def finish(ok):
 report['passed']=ok
 (out/'visual.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 levels.editor_request_end_play();s.update(phase=99,next=time.monotonic()+2)

views=[
 ('01-ramp-to-stage.png',(30200,11750,20100),(31100,11750,20400)),
 ('02-stage-to-passage.png',(31550,11750,20180),(32850,11750,20130)),
 ('03-stage-roof-and-side.png',(31900,11750,20320),(32500,13250,21600)),
 ('04-stage-back-to-tunnel.png',(32000,11750,20200),(30200,11750,20150)),
]

def tick(dt):
 if s.get('busy'):return
 s['busy']=True
 try:
  now=time.monotonic()
  if now<s['next']:return
  phase=s['phase']
  if phase==99:
   unreal.unregister_slate_post_tick_callback(handle)
   unreal.EditorPythonScripting.set_keep_python_script_alive(False)
   unreal.SystemLibrary.quit_editor();return
  if now-s['start']>180:raise RuntimeError('Stage render timeout')
  if phase==0:
   helper.start_floating_preview();s.update(phase=1,next=now+4);return
  w=editor.get_game_world()
  if not w:return
  if phase==1:
   hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
   if not hall.get_editor_property('bReady'):return
   for opening in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening):opening.start_game()
   s.update(phase=10,next=now+5);return
  if phase==10:
   hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
   pawn=unreal.GameplayStatics.get_player_pawn(w,0)
   pc=unreal.GameplayStatics.get_player_controller(w,0)
   assert helper.prepare_collection_preview_area(pc,3,True)
   hall.set_actor_tick_enabled(False)
   assert helper.resize_preview_viewport(pc,1280,800)
   e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0]
   e.set_actor_tick_enabled(False)
   e.get_editor_property('stage_celebration').set_actor_tick_enabled(True)
   pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
   shell=next(c for c in e.get_components_by_class(unreal.DynamicMeshComponent) if c.component_has_tag('EscapeStageBedrock'))
   assert shell.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION
   s.update(pc=pc,pawn=pawn,e=e,shell=shell,phase=2,next=now+1);return
  if phase==2:
   # World-space pawn sweeps above the railings, at the ramp and stage.
   for x in (30400,31500,32500):
    for delta in ((0,-6000,0),(0,6000,0),(0,0,6000),(0,0,-6000)):
     start=unreal.Vector(x,11750,20500)
     hit=unreal.SystemLibrary.capsule_trace_single(w,start,start+unreal.Vector(*delta),42,96,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
     assert hit and hit.to_tuple()[0],(x,delta)
     report['world_sweeps'].append({'x':x,'direction':delta,'blocked':True})
   for y in (-80,0,80):
    hit=unreal.SystemLibrary.capsule_trace_single(w,unreal.Vector(32500,11750+y,20001),unreal.Vector(33000,11750+y,20001),42,96,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
    assert not hit or not hit.to_tuple()[0],'Stage/cave walking aperture blocked at '+str(y)
   report['third_passage_join_walkable']=True
   facing=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.GeneratedMiningRock) if a.actor_has_tag('EscapeStageRedFacing'))
   report['facing_transform']=str(facing.get_actor_transform())
   report['triangles_before']=facing.get_editor_property('triangle_count')
   assert facing.mine_sphere(unreal.Vector(31500,9950,20500),450)
   s.update(facing=facing,phase=21,next=now+2);return
  if phase==21:
   f=s['facing'];m=f.get_component_by_class(unreal.DynamicMeshComponent)
   trace=m.line_trace_component(unreal.Vector(31500,11750,20500),unreal.Vector(31500,9250,20500),True,False,False)
   report['cut_probe']=str(trace)
   assert not trace,'Excavation must open the red facing completely'
   report['cut_status']=str(f.get_editor_property('last_status'))
   report['triangles_after']=f.get_editor_property('triangle_count')
   report['successful_cuts']=f.get_editor_property('successful_cuts')
   assert report['successful_cuts']>0
   backing=s['shell'].line_trace_component(unreal.Vector(31500,11750,20500),unreal.Vector(31500,9250,20500),True,False,False)
   assert backing,'Bedrock must remain behind the excavated facing'
   report['bedrock_after_cut']=str(backing)
   f.reset_rock()
   pose(views[0][1],views[0][2]);s.update(phase=3,view=0,next=now+3);return
  if phase==3:
   i=s['view'];name=views[i][0]
   assert helper.capture_preview(s['pc'],str(out/name))
   report['screenshots'].append(name)
   if i+1==len(views):finish(True);return
   pose(views[i+1][1],views[i+1][2]);s.update(view=i+1,next=now+3)
 except Exception:
  report['error']=traceback.format_exc();unreal.log_error(report['error']);finish(False)
 finally:s['busy']=False

handle=unreal.register_slate_post_tick_callback(tick)
