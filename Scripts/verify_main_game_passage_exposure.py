"""Verify corridor visibility facing both the mine and the authored room."""
import unreal,json,time,traceback,sys,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGamePassageExposure';OUT.mkdir(exist_ok=True);sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
state={'phase':'setup','next':0,'start':time.monotonic(),'i':0,'view':0,'busy':False};report={'passed':False,'checks':[],'views':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def finish(error=None):
 report.update(passed=error is None,error=error,elapsed=time.monotonic()-state['start']);(OUT/'runtime.json').write_text(json.dumps(report,indent=2));perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def contains(a,p,padding=0):
 c=a.get_component_by_class(unreal.BoxComponent);return all(abs(x-y)<e+padding for x,y,e in zip(p.to_tuple(),c.get_world_location().to_tuple(),c.get_scaled_box_extent().to_tuple()))
def tick(dt):
 if state['busy']:return
 state['busy']=True
 try:
  now=time.monotonic();assert now-state['start']<180,'Passage visibility audit timed out'
  if now<state['next']:return
  world=ed.get_game_world()
  if not world:return
  if state['phase']=='setup':
   halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall);pc=unreal.GameplayStatics.get_player_controller(world,0)
   if not halls or not halls[0].get_editor_property('bReady') or not pc or not pc.mining_gui:return
   hall=halls[0];pawn=unreal.GameplayStatics.get_player_pawn(world,0);zones=list(unreal.GameplayStatics.get_all_actors_with_tag(world,'BM_MainGame_PassageExposure'));state.update(hall=hall,pc=pc,pawn=pawn,zones=zones,move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),phase='place')
   check(len(zones)==6,'Exactly six passage exposure zones, excluding clay plugs')
   for zone in zones:
    c=zone.get_component_by_class(unreal.PostProcessComponent);s=c.settings
    check(c.enabled and not c.unbound and c.priority==10 and c.blend_radius==200,'Adaptive passage profile is local and always enabled')
    check(s.auto_exposure_method==unreal.AutoExposureMethod.AEM_HISTOGRAM and s.auto_exposure_min_brightness==-2 and s.auto_exposure_max_brightness==4,'Passages adapt between EV100 -2 and 4')
    check(s.auto_exposure_low_percent==90 and s.auto_exposure_high_percent==98 and abs(s.local_exposure_highlight_contrast_scale-.5)<.001 and abs(s.local_exposure_shadow_contrast_scale-.35)<.001,'Highlight metering and local shadow detail preserve bright-doorway contrast')
    check(zone.get_component_by_class(unreal.BoxComponent).get_collision_enabled()==unreal.CollisionEnabled.QUERY_ONLY or zone.get_component_by_class(unreal.BoxComponent).get_collision_enabled()==unreal.CollisionEnabled.QUERY_AND_PHYSICS,'Passage bounds support exposure queries')
   for p in cfg['mine_origins']:
    check(not any(contains(a,unreal.Vector(*p)+unreal.Vector(0,0,3000),200) for a in zones),'Passage profiles do not change mine interiors')
   for s in cfg['generated_connections']:
    for j in range(17):
     x=s['a'][0]+(s['b'][0]-s['a'][0])*j/16;p=unreal.Vector(x,s['a'][1],geometry.floor_at(s,x)+160)
     assert any(contains(a,p) for a in zones),(s['id'],j,'Gap in adaptive corridor coverage')
    check(True,s['id']+' has uninterrupted exposure coverage across its full length')
   return
  if state['phase']=='place':
   s=cfg['generated_connections'][state['i']];hall=state['hall'];hall.set_editor_property('linear_campaign',False);assert hall.enter_area(s['mine']*2);hall.set_editor_property('linear_campaign',True)
   if state['view']>=2:
    clay=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.get_actor_label()=='LINK_'+s['id']+'_DiggableSoil');assert clay.mine_blast(clay.get_actor_location(),400,.05,61000+state['i']) or clay.successful_cuts>0
   x=(s['a'][0]+s['b'][0])/2;p=unreal.Vector(x,s['a'][1],geometry.floor_at(s,x)+100);state['pawn'].set_actor_location(p,False,True);state['move'].stop_movement_immediately();state['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
   mine_facing=state['view'] in [0,3];yaw=(180 if s['side']=='exit' else 0) if mine_facing else (0 if s['side']=='exit' else 180);state['pc'].set_control_rotation(unreal.Rotator(yaw=yaw));state.update(phase='capture',next=now+2);return
  if state['phase']=='capture':
   s=cfg['generated_connections'][state['i']];name=['mine_blocked','room_blocked','room_open','mine_open'][state['view']];path=OUT/(s['id']+'_'+name+'.png');check(helper.capture_preview(state['pc'],str(path)),s['id']+' capture '+name);report['views'].append({'id':s['id'],'view':name,'path':str(path)});state['view']+=1
   if state['view']==4:state['i']+=1;state['view']=0
   if state['i']==6:finish()
   else:state.update(phase='place',next=now+.1)
 except Exception:finish(traceback.format_exc())
 finally:state['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
