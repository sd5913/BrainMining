"""Saved MainGame geometry, bomb-only gates, parkour landings and museum drop in PIE."""
import unreal,json,time,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV5';ART=ROOT/'Art/MainGame/RoutesV5';sys.path.insert(0,str(ROOT/'Scripts'))
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
render='-nullrhi' not in unreal.SystemLibrary.get_command_line().lower();report={'passed':False,'checks':[],'regions':[]};s={'phase':'setup','start':time.monotonic(),'next':0,'busy':False,'index':0}
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,msg):
 assert ok,msg
 report['checks'].append(msg)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(a,b,ignore=None):
 h=unreal.SystemLibrary.line_trace_single(s['world'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore or [s['pawn']],unreal.DrawDebugTrace.NONE,True);return h.to_tuple() if h else None
def move(p,target=None,fall=False):
 s['pawn'].set_actor_location(p,False,True);s['movement'].stop_movement_immediately();s['movement'].set_movement_mode(unreal.MovementMode.MOVE_FALLING if fall else unreal.MovementMode.MOVE_FLYING)
 if target:s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),target))
def enter(index):
 hall=s['hall'];hall.set_editor_property('linear_campaign',False)
 try:check(hall.enter_area(index),'Activate area '+str(index))
 finally:hall.set_editor_property('linear_campaign',True)
def shot(name):
 if render:check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Screenshot '+name)
def finish(error=None):
 report.update(passed=error is None,error=error,phase=s['phase'],elapsed=time.monotonic()-s['start'],rendered=render)
 (OUT/('runtime-rendered.json' if render else 'runtime.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old);levels.editor_request_end_play();s.update(phase='done',next=time.monotonic()+1)
def tick(dt):
 if s['busy'] or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  now=time.monotonic()
  if s['phase']=='done':
   if not ed.get_game_world():unreal.unregister_slate_post_tick_callback(callback);unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
   return
  assert now-s['start']<420,'Verification timeout'
  world=ed.get_game_world()
  if not world:return
  phase=s['phase']
  if phase=='setup':
   pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
   if not pawn or not pc or not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui or not pc.mining_gui.get_editor_property('bMineralProgression'):return
   hall=halls[0];hall.set_actor_tick_enabled(False)
   s.update(world=world,pawn=pawn,pc=pc,hall=hall,movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent),mining=pawn.get_component_by_class(unreal.MiningComponent))
   s['regions']=[next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) if a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}')) for i in range(3)]
   check(len(hall.areas)==6 and len(hall.treasure.items)==139,'Three mines, original rooms and 139 mineral deposits retained')
   check(pc.mining_gui.get_editor_property('bMineralProgression'),'Persistent mineral bag and pickaxe HUD initialized before the geometry audit')
   bomb=s['bomb'];check(bomb.max_bombs==1 and bomb.bomb_count<=1,'Bomb capacity is one');check(bomb.settings.blast_radius<=350 and bomb.settings.excavation_radius<=260 and bomb.settings.explosion_scale<=.625,'Both bomb radii and visual scale are at most half their former values')
   lights=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Light) if a.actor_has_tag('BM_RoutesV5')]
   check(len(lights)>5 and all(a.actor_has_tag('BM_Campaign_MainGame02') for a in lights),'Environmental cave lights belong only to mine 2')
   s['platforms']=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor) if a.actor_has_tag('BM_ParkourPlatform')];check(len(s['platforms'])==10,'Two chambers have five indestructible grounded platforms each')
   s['gates']=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.actor_has_tag('BM_BombOnly')];check(len(s['gates'])==6,'Six bomb-only gates start at the office exit')
   s['soil']=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.actor_has_tag('BM_MuseumDownwardSoil'))
   s['ignore']=[pawn,*hall.treasure.items,*s['platforms'],s['soil']]+[g for r in s['regions'] for g in r.generated_guide_layers]
   s.update(phase='pickup_aim',pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),pickup=0,pickup_attempt=0,next=now+3,room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0])
  elif phase=='pickup_aim':
   p=s['pickups'][s['pickup']].get_actor_location();move(unreal.Vector(p.x-125,p.y,s['room'].get_actor_location().z+102),p);s.update(phase='pickup_press',next=now+.6)
  elif phase=='pickup_press':
   helper.send_preview_key(s['pc'],'E',True);s.update(phase='pickup_release',next=now+.15)
  elif phase=='pickup_release':
   helper.send_preview_key(s['pc'],'E',False)
   if not s['pickups'][s['pickup']].collected:
    s['pickup_attempt']+=1;assert s['pickup_attempt']<5,'Starter pickup input did not register';s.update(phase='pickup_aim',next=now+1);return
   check(True,'Starter tool collected '+str(s['pickup']));s['pickup']+=1;s['pickup_attempt']=0;s.update(phase='pickup_aim' if s['pickup']<len(s['pickups']) else 'region',next=now+.3)
  elif phase=='region':
   i=s['index'];r=s['regions'][i];enter(i*2);origin=r.get_actor_location()+r.cutters[0].local_transform.translation;paths=json.loads((ART/STAGES[i]/'centerlines.json').read_text());s.update(region=r,origin=origin,paths=paths)
   # Remote geometry audit loads collision deliberately; a separate input/FX audit uses normal lazy loading.
   count=0
   for path in paths:
    for k in range(9,len(path['points'])-9,9):
     if path['name']=='Main' and (path['arc'][k]<12 or path['arc'][k]>path['arc'][-1]-15):continue
     a=origin+raw(path['points'][k]);b=origin+raw(path['points'][k+2]);h=unreal.SystemLibrary.capsule_trace_single(world,a,b,42,90,unreal.TraceTypeQuery.ECC_VISIBILITY,True,s['ignore'],unreal.DrawDebugTrace.NONE,True)
     h=h.to_tuple() if h else None
     assert not h or not h[0],(STAGES[i],path['name'],k,'Capsule blocked',h[9].get_actor_label())
     count+=1
   check(count>40,STAGES[i]+' capsule clearance at '+str(count)+' positions');report['regions'].append(dict(stage=STAGES[i],capsule_samples=count))
   main=paths[0];summary=json.loads((ART/STAGES[i]/'plan_summary.json').read_text());arc=summary['cavern_centers_arc_m'][0] if i else 35;k=min(range(len(main['arc'])),key=lambda k:abs(main['arc'][k]-arc));p=origin+raw(main['points'][k]);target=origin+raw(main['points'][min(k+28,len(main['points'])-1)])
   if i==1:p=origin+raw([-37,-5,71.8]);target=origin+raw([-23,0,78])
   if i==2:
    arc=summary['cavern_centers_arc_m'][1];k=min(range(len(main['arc'])),key=lambda k:abs(main['arc'][k]-arc));p=origin+raw(main['points'][max(0,k-15)])-unreal.Vector(0,0,180);target=origin+raw(json.loads((ART/STAGES[i]/'neural_rocks.json').read_text())[0]['probe_m'])
   move(p-unreal.Vector(0,0,60),target);s.update(phase='photo',next=now+1)
  elif phase=='photo':
   shot('mine_'+str(s['index']+1));s['index']+=1;s.update(phase='region' if s['index']<3 else 'platforms',next=now+.1)
  elif phase=='platforms':
   features=json.loads((ART/STAGES[1]/'features.json').read_text());r=s['regions'][1];origin=r.get_actor_location()+r.cutters[0].local_transform.translation
   for p in features['platforms']:
    top=origin+raw(p['top_m']);h=trace(top+unreal.Vector(0,0,50),top-unreal.Vector(0,0,50));check(h and h[0] and h[9] in s['platforms'] and abs(h[5].z-top.z)<3,'Collision on landing '+p['name'])
    check(p['base_z_m']<p['top_m'][2]-3,'Grounded column '+p['name'])
   enter(2);top=origin+raw(features['platforms'][0]['top_m']);s['takeoff']=top+unreal.Vector(0,0,100);move(s['takeoff'],fall=True);s['movement'].set_flight_input_held(True);s.update(phase='jetpack_rise',next=now+1.1)
  elif phase=='jetpack_rise':
   check(s['pawn'].get_actor_location().z>s['takeoff'].z+240,'Real jetpack thrust rises from a parkour platform');s['movement'].set_flight_input_held(False);s.update(phase='jetpack_land',next=now+2)
  elif phase=='jetpack_land':
   check(abs(s['pawn'].get_actor_location().z-s['takeoff'].z)<40,'Release jetpack and land on the indestructible platform');s.update(phase='hard_gates')
  elif phase=='hard_gates':
   # Gate tools operate directly on the actual serialized actors, including chunk variants.
   for gate in s['gates']:
    center,extent=gate.get_actor_bounds(False);before=gate.successful_cuts
    check(not gate.mine_sphere(center,500),'Drill rejected '+gate.get_actor_label());check(not gate.mine_pickaxe(unreal.Transform(location=center),unreal.Vector(500,500,500),31,.3),'Pickaxe rejected '+gate.get_actor_label());check(gate.successful_cuts==before,'Rejected tools preserve '+gate.get_actor_label())
   gate=next(g for g in s['gates'] if 'OfficeExit' in g.get_actor_label());center,extent=gate.get_actor_bounds(False);s['blast_gate']=gate;s['before_cuts']=gate.successful_cuts
   enter(2);move(center-unreal.Vector(400,0,0),center)
   check(s['bomb'].toggle_equipped(),'Equip real bomb tool');check(s['bomb'].throw_bomb(),'Throw the single available bomb');check(s['bomb'].bomb_count==0 and not s['bomb'].throw_bomb(),'A second bomb cannot be thrown while recharging')
   thrown=s['bomb'].last_thrown_bomb;thrown.collision.set_simulate_physics(False);thrown.set_actor_location(center,False,True);thrown.explode();s.update(phase='blast',next=now+.5)
  elif phase=='blast':
   gate=s['blast_gate']
   if getattr(gate,'cut_pending',False):return
   check(gate.successful_cuts>s['before_cuts'],'Actual reduced-radius bomb removes first hard gate geometry');gate.reset_rock();s['bomb'].stow();enter(4)
   move(unreal.Vector(47200,11750,15300),unreal.Vector(47200,11750,14800));s.update(phase='soil',next=now+.5)
  elif phase=='soil':
   h=trace(unreal.Vector(47200,11750,15300),unreal.Vector(47200,11750,14700));check(h and h[0] and h[9]==s['soil'],'Downward route ends at real diggable soil above museum')
   shot('museum_downward_soil');check(s['soil'].mine_pickaxe(unreal.Transform(location=unreal.Vector(47200,11750,15000)),unreal.Vector(150,150,200),19,.35),'Pickaxe opens the downward soil');s.update(phase='fall',next=now+.3)
  elif phase=='fall':
   h=trace(unreal.Vector(47200,11750,15100),unreal.Vector(47200,11750,14300));check(h and h[0] and h[5].z<14600,'Opened shaft reaches museum floor without an invisible roof collider')
   s['hall'].set_actor_tick_enabled(True);move(unreal.Vector(47200,11750,15170),fall=True);s.update(phase='landed',next=now+2)
  elif phase=='landed':
   check(s['hall'].active_area==5,'Falling through ceiling activates museum');check(s['pawn'].get_actor_location().z<14700,'Player physically falls inside museum');shot('museum_landing');finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'bm.Mining.LazyRuntimeChunks 0')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
