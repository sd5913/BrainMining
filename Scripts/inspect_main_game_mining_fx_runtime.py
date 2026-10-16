"""Observe genuine mining impacts at several MainGame route positions in PIE."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameMiningFX';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
previous_lazy=unreal.SystemLibrary.get_console_variable_int_value('bm.Mining.LazyRuntimeChunks')
STRICT=globals().get('STRICT',False)
s={'phase':'setup','next':0,'start':time.monotonic(),'pickup':0,'index':0,'busy':False};report={'completed':False,'passed':False,'checks':[],'samples':[]};cases=globals().get('cases',[(0,.23,False),(0,.8,True),(1,.8,False),(1,.8,True)])
REPORT_FILE='runtime.json' if STRICT else 'observations.json'
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def key(n,down):helper.send_preview_key(s['pc'],n,down)
def press(n):key(n,True);key(n,False)
def aim(p):s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),p))
def move(p):s['pawn'].set_actor_location(p,False,True);s['movement'].stop_movement_immediately();s['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
def record():
    bursts=[];clouds=[]
    for a in unreal.GameplayStatics.get_all_actors_of_class(s['world'],unreal.MiningDebrisBurst):
        chunks=list(a.get_components_by_class(unreal.StaticMeshComponent));visible=[c for c in chunks if c.is_visible()]
        bursts.append(dict(hidden=a.get_editor_property('hidden'),chunks=len(chunks),visible=len(visible),active=a.get_editor_property('chunk_count'),material=str(a.get_editor_property('source_material')),location=list(a.get_actor_location().to_tuple())))
    for a in unreal.GameplayStatics.get_all_actors_of_class(s['world'],unreal.MiningDrillDustBurst):clouds.append(dict(hidden=a.get_editor_property('hidden'),tick=a.is_actor_tick_enabled(),tags=[str(t) for t in a.tags],dust=a.sprites.get_instance_count(),smoke=a.smoke_sprites.get_instance_count(),sparks=a.spark_sprites.get_instance_count(),location=list(a.get_actor_location().to_tuple())))
    r=dict(case=cases[s['index']],area=s['hall'].active_area,bursts=bursts,clouds=clouds,burst_delta=s['fx'].debris_burst_count-s['bursts'],successful_cuts=s['rock'].successful_cuts-s['cuts'],status=s['mining'].get_editor_property('status'),target=str(s['mining'].get_editor_property('target_rock')))
    report['samples'].append(r)
    if STRICT:
        check(r['burst_delta']>0 and r['successful_cuts']>0,'Real input excavates terrain and emits fragments '+str(cases[s['index']]))
        check(any(b['active']>0 and b['visible']>0 and not b['hidden'] and b['material']!='None' for b in bursts),'Matching material fragments stay visible '+str(cases[s['index']]))
        check(len(bursts)<=4 and len(clouds)<=12,'Impact accumulation remains bounded '+str(cases[s['index']]))
        if cases[s['index']][2]:
            check(bool(clouds) and all(c['tick'] and not c['hidden'] and 'BM_PlayerEffect' in c['tags'] for c in clouds),'Drill dust is excluded from neighbouring-area suspension '+str(cases[s['index']]))
            check(all(c['dust']>0 and c['smoke']>0 and c['sparks']>0 for c in clouds),'Dust, smoke and sparks are present '+str(cases[s['index']]))
            s['cloud_probes']=[(a,a.sprites.get_instance_transform(0,True).translation) for a in unreal.GameplayStatics.get_all_actors_of_class(s['world'],unreal.MiningDrillDustBurst)]
    (OUT/REPORT_FILE).write_text(json.dumps(report,indent=2),encoding='utf-8');helper.capture_preview(s['pc'],str(OUT/(('verified_' if STRICT else 'observed_')+'%02d.png'%s['index'])))
def advance(now):
    s['index']+=1
    if s['index']==len(cases):
        if STRICT:s['mining'].reset_field();s.update(phase='cleanup',next=now+.3)
        else:finish()
    else:s.update(phase='prepare',next=now+.4)
def finish(error=None):
    if 'pc' in s:
        for n in ['E','V','LeftMouseButton']:key(n,False)
    report.update(completed=error is None,passed=STRICT and error is None,elapsed=time.monotonic()-s['start'])
    if error:report['error']=error
    (OUT/REPORT_FILE).write_text(json.dumps(report,indent=2),encoding='utf-8');s['phase']='done';perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'bm.Mining.LazyRuntimeChunks %d'%previous_lazy);unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
    if s['busy'] or s['phase']=='done':return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<180,'FX inspection timeout'
        if now<s['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=s['phase']
        if phase=='setup':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not pc or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady'):return
            s.update(world=world,pawn=pawn,pc=pc,hall=halls[0],fx=pawn.get_component_by_class(unreal.MiningToolEffectsComponent),mining=pawn.get_component_by_class(unreal.MiningComponent),movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0],phase='aim_pickup')
        elif phase=='aim_pickup':
            p=s['pickups'][s['pickup']].get_actor_location();move(unreal.Vector(p.x-125,p.y,s['room'].get_actor_location().z+102));aim(p);s.update(phase='press_pickup',next=now+.6)
        elif phase=='press_pickup':key('E',True);s.update(phase='pickup',next=now+.15)
        elif phase=='pickup':
            key('E',False);assert s['pickups'][s['pickup']].collected;s['pickup']+=1;s.update(phase='aim_pickup' if s['pickup']<2 else 'prepare',next=now+.3)
        elif phase=='prepare':
            if STRICT and not s.get('upgraded'):
                s['pawn'].get_component_by_class(unreal.MiningProgressionComponent).get_wallet().add_coins(100000)
                for level in range(1,5):check(s['hall'].purchase_upgrade(0,level),'Fixture buys real pickaxe level '+str(level))
                s['upgraded']=True
            case=cases[s['index']];i,f,drill=case[:3];guide=len(case)>3 and case[3]=='guide';s['hall'].set_editor_property('linear_campaign',False)
            try:assert s['hall'].enter_area(i*2)
            finally:s['hall'].set_editor_property('linear_campaign',True)
            # Restore the preceding test's cavity before locating the next wall.
            s['mining'].reset_field()
            region=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) if a.actor_has_tag('BM_Campaign_MainGame%02d'%(i+1)));stage=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4'][i];paths=json.loads((ROOT/'Art/MainGame/RoutesV3'/stage/'centerlines.json').read_text(encoding='utf-8'));path=paths[0];origin=region.get_actor_location()+region.cutters[0].local_transform.translation
            if guide:
                spec=json.loads((ROOT/'Art/MainGame/RoutesV3'/stage/'placement_plan.json').read_text(encoding='utf-8'))['objects'][0];path=paths[spec['path_index']];k=min(range(len(path['arc'])),key=lambda n:abs(path['arc'][n]-(spec['start_arc_m']-6)));p=origin+raw(path['points'][k]);end=origin+raw(spec['center_m'])
            else:
                k=int(len(path['points'])*f);p=origin+raw(path['points'][k]);t=raw(path['points'][k+1])-raw(path['points'][k-1]);side=unreal.Vector(-t.y,t.x,0);end=p+side.normal()*3000
            ignored=[s['pawn']]+list(s['hall'].treasure.items)
            if not guide:ignored+=[g for r in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) for g in r.generated_guide_layers]
            h=unreal.SystemLibrary.line_trace_single(world,p,end,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True).to_tuple();assert h[0] and isinstance(h[9],unreal.DiggableRock),(stage,str(h));assert (h[9] in region.generated_guide_layers) if guide else h[9]==region.generated_rock
            s.update(rock=h[9],point=h[5],normal=h[6]);move(h[5]+h[6]*160-unreal.Vector(0,0,64));aim(h[5]);s.update(phase='hit',next=now+.8)
        elif phase=='hit':
            drill=cases[s['index']][2]
            if s['mining'].get_editor_property('bDrillMode')!=drill:press('V')
            s.update(bursts=s['fx'].debris_burst_count,cuts=s['rock'].successful_cuts,phase='observe',next=now+.65);key('LeftMouseButton',True)
        elif phase=='observe':
            record();key('LeftMouseButton',False)
            if STRICT and cases[s['index']][2]:s.update(phase='motion',next=now+.12)
            else:advance(now)
        elif phase=='motion':
            check(any(unreal.SystemLibrary.is_valid(a) and a.sprites.get_instance_transform(0,True).translation.distance(before)>.01 for a,before in s['cloud_probes']),'Dust sprites continue moving after their spawn frame '+str(cases[s['index']]));advance(now)
        elif phase=='cleanup':
            check(not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningDrillDustBurst),'Reset removes drill contact effects')
            check(all(a.chunk_count==0 and a.get_editor_property('hidden') for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningDebrisBurst)),'Reset recycles fragments without leaving visible or active debris');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'bm.Mining.LazyRuntimeChunks 0')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
