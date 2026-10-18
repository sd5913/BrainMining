"""Isolated P2 rendered PIE fixture: real screen trace, cuts, safety, stage and E pickup.
Poses are fixtures; this does not certify a complete human parkour playthrough.
"""
import unreal,time,json,traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardP2Flow';out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
s={'phase':0,'start':time.monotonic(),'next':0};report={'passed':False,'checks':[]}
def check(v,label):
 if not v:raise AssertionError(label)
 report['checks'].append(label)
def shot(n):check(helper.capture_preview(s['pc'],str(out/n)),'Captured '+n)
def pose(p,t):
 s['pawn'].set_actor_location(p,False,True)
 s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],t))
def rock_center(rock):
 return unreal.SystemLibrary.get_component_bounds(rock.get_component_by_class(unreal.DynamicMeshComponent))[0]
def finish(ok,reason):
 report.update(passed=ok,reason=reason,elapsed=time.monotonic()-s['start'])
 (out/'visual.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 unreal.log('CARD_P2_VISUAL '+str(ok)+' '+reason)
 levels.editor_request_end_play();s.update(phase=99,next=time.monotonic()+2)
def tick(dt):
 if s.get('busy'):return
 s['busy']=True
 try:
  now=time.monotonic()
  if now<s['next']:return
  phase=s['phase']
  if phase==99:
   unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
  if now-s['start']>240:raise RuntimeError('Timeout at '+str(phase))
  if phase==0:helper.start_floating_preview();s.update(phase=1,next=now+3);return
  w=editor.get_game_world()
  if not w:return
  if phase==1:
   pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
   es=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)
   if not pawn or not es:return
   e=es[0];stage=e.get_editor_property('card_stage');move=pawn.get_component_by_class(unreal.CharacterMovementComponent)
   s.update(w=w,pc=pc,pawn=pawn,e=e,move=move,mine=pawn.get_component_by_class(unreal.MiningComponent))
   pawn.set_actor_location(unreal.Vector(12060,3600,3100),False,True)
   e.call_method('OnRewardClaimed',args=(stage.get_editor_property('room_id'),));e.retry_escape();move.disable_movement()
   pose(unreal.Vector(16500,4500,3200),unreal.Vector(18000,10000,2000))
   s.update(phase=2,next=now+1);return
  e=s['e'];entry=e.get_editor_property('computer_entry');end=e.get_editor_property('tunnel_end');axis=(end-entry)/(end-entry).length()
  if phase==2:
   props=list(e.get_editor_property('floating_props'))
   check(all(c.is_visible() for c in props),'Floats are visible during initial water surge')
   tops=[]
   for i in range(6):
    parts=[c for c in props if c.component_has_tag('EscapeFloatingPad'+str(i))]
    check(bool(parts),'Floating step '+str(i)+' has geometry')
    tops.append(max(unreal.SystemLibrary.get_component_bounds(c)[0].z+unreal.SystemLibrary.get_component_bounds(c)[1].z for c in parts))
   report['initial_float_tops']=tops
   check(all(b>a+70 for a,b in zip(tops,tops[1:])),'All six floating steps rise progressively')
   shot('01-floats-and-tv.png')
   pose(entry+unreal.Vector(-1700,-600,-100),entry)
   s.update(phase=3,next=now+1);return
  if phase==3:
   shot('02-authored-tv.png')
   screen=e.get_editor_property('television_screen');center=rock_center(screen)
   s['screen']=screen;s['cuts_before']=screen.get_editor_property('successful_cuts')
   # Camera at screen centre, within normal mining range; actual trace must hit authored glass.
   pose(center+unreal.Vector(-300,0,-64),center)
   s.update(phase=4,next=now+.3);return
  if phase==4:
   cam=s['pc'].player_camera_manager
   start=cam.get_camera_location();rotation=cam.get_camera_rotation();end_trace=start+unreal.MathLibrary.get_forward_vector(rotation)*500
   report['screen_trace']=str(unreal.SystemLibrary.line_trace_single(w,start,end_trace,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True))
   report['screen_pose']={'camera':str(start),'end':str(end_trace),'bounds':str(s['screen'].get_actor_bounds(False)),'components':[str(c.get_collision_enabled()) for c in s['screen'].get_components_by_class(unreal.DynamicMeshComponent)]}
   s['mine'].try_mine();s['strikes']=s.get('strikes',0)+1;s.update(phase=5,next=now+.8);return
  if phase==5:
   if s['screen'].get_editor_property('successful_cuts')<=s['cuts_before'] and s['strikes']<12:
    s.update(phase=4,next=now+.1);return
   report['screen_mining']={'strikes':s['strikes'],'status':str(s['mine'].get_editor_property('status')),'target':str(s['mine'].get_editor_property('target_rock'))}
   shot('03-screen-cut.png')
   check(s['screen'].get_editor_property('successful_cuts')>s['cuts_before'],'Normal mining trace excavates the restored green TV screen')
   s['rocks']=list(e.get_editor_property('computer_rocks'))
   pose(entry+unreal.Vector(-650,0,0),entry+axis*900)
   # Traverse with actual capsule sweeps and the real drill trace. No MineSphere fixtures.
   s['mine'].set_editor_property('drill_consumption_rate',0)
   s.update(phase=6,route=[entry]+[entry+axis*d for d in range(50,7251,50)],route_index=0,stalled=now,drill_attempts=0,next=now+.1);return
  if phase==6:
   if s['route_index']<len(s['route']):
    target=s['route'][s['route_index']];pos=s['pawn'].get_actor_location()
    direction=(target-pos)/(target-pos).length() if (target-pos).length()>1 else axis
    eyes=s['pawn'].get_actor_eyes_view_point()[0]
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eyes,eyes+direction*1000))
    s['mine'].try_mine();s['drill_attempts']+=1
    dest=pos+direction*min(50,(target-pos).length())
    hit=s['pawn'].set_actor_location(dest,True,False)
    moved=(s['pawn'].get_actor_location()-pos).length()
    if moved>2:s['stalled']=now
    if now-s['stalled']>10:raise RuntimeError('Normal drill traversal blocked: '+str(hit.to_tuple())+' target='+str(s['mine'].get_editor_property('target_rock')))
    if (s['pawn'].get_actor_location()-target).length()<3:s['route_index']+=1
    if s['route_index']>=15 and 'safe_water' not in s:
     check(e.get_editor_property('bEnteredComputer'),'Actual screen crossing enters safe television')
     s['safe_water']=e.get_editor_property('water_z');shot('04-blue-interior.png')
    s['next']=now+.13;return
   report['drill_traversal']=dict(waypoints=s['route_index'],attempts=s['drill_attempts'],cuts=[r.get_editor_property('successful_cuts') for r in s['rocks']])
   s['entry_clear']=True
   check(all(r.get_editor_property('successful_cuts')>0 for r in s['rocks']),'Normal drill excavates every contiguous tunnel slab')
   check(abs(e.get_editor_property('water_z')-s['safe_water'])<.1,'Water remains frozen throughout safe TV excavation')
   pose(end+unreal.Vector(100,0,50),unreal.Vector(31200,11750,10400))
   s.update(phase=8,next=now+.8);return
  if phase==8:
   shot('05-exit-red-curtain.png')
   curtains=[c for c in e.get_components_by_class(unreal.StaticMeshComponent) if c.component_has_tag('EscapeStageCurtain')]
   check(bool(curtains) and all(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for c in curtains),'Red curtain is walk-through')
   pose(unreal.Vector(30800,11750,10030),unreal.Vector(31900,11750,10400));s.update(phase=13,next=now+1.2);return
  if phase==13:
   effects=e.get_editor_property('stage_celebration')
   leaves=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor) if a.get_class().get_name()=='Actor' and a.get_owner()==e and abs(a.get_actor_location().x-31070)<1]
   check(len(leaves)==2 and all(abs(a.get_actor_location().y-11750)>1000 for a in leaves),'Both curtain leaves physically slide clear of the doorway')
   check(not effects.get_editor_property('bStageActive'),'Opening curtain does not start cannons')
   shot('05a-curtain-opening.png')
   pose(e.get_editor_property('exit'),unreal.Vector(31900,11750,10100));s.update(phase=9,next=now+2);return
  if phase==9:
   check(e.get_editor_property('phase')==unreal.MiningEscapePhase.ESCAPED,'Cut and crossed tunnel reaches stage')
   effects=e.get_editor_property('stage_celebration')
   check(effects.get_editor_property('bCurtainOpen'),'Approaching from the television opens the curtain')
   check(not effects.get_editor_property('bStageActive'),'Confetti stays quiet at the curtain threshold')
   shot('05b-curtain-open-before-confetti.png')
   pose(e.get_editor_property('exit')+unreal.Vector(400,0,0),unreal.Vector(32500,11750,10250));s.update(phase=12,next=now+2.6);return
  if phase==12:
   effects=e.get_editor_property('stage_celebration')
   check(effects.get_editor_property('bStageActive'),'Stage arrival activates celebration')
   check(sum(c.get_instance_count() for c in effects.get_components_by_class(unreal.InstancedStaticMeshComponent))==640,'Two cannons reuse 640 confetti pieces')
   shot('06-stage-celebration.png')
   ribbon=e.get_editor_property('stage_ribbon');s['ribbon']=ribbon
   check(ribbon.get_editor_property('item_id')==11,'Stage reuses collection ribbon model')
   pose(ribbon.get_actor_location()+unreal.Vector(-180,0,100),ribbon.get_actor_location()+unreal.Vector(0,0,12))
   s.update(phase=10,next=now+1);return
  if phase==10:
   fields=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)
   field=next(f for f in fields if s['ribbon'] in f.get_editor_property('items'));s['field']=field
   check(field.get_editor_property('target')==s['ribbon'],'Ribbon is aimable through normal collection trace')
   s['owned_before']=field.get_editor_property('inventory')[11]
   shot('07-ribbon-pickup.png');helper.send_preview_key(s['pc'],'E',True);helper.send_preview_key(s['pc'],'E',False)
   s.update(phase=11,next=now+.5);return
  if phase==11:
   check(s['ribbon'].get_editor_property('bCollected'),'E collects the stage ribbon')
   check(s['field'].get_editor_property('inventory')[11]==s['owned_before']+1,'Collection inventory gains exactly one ribbon')
   check(s['entry_clear'],'Player capsule sweeps through excavated glass, cabinet and wall into tunnel')
   finish(True,'P2 rendered interaction fixture passed')
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
