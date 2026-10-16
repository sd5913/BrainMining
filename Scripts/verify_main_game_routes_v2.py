"""Saved-map corridor, landmark, excavation and PIE presentation acceptance."""
import unreal,json,time,traceback,math,sys
from pathlib import Path
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/'Scripts'))
from main_game_route_sources import tuning
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV2';ART=ROOT/'Art/MainGame/RoutesV2'
TUNING=tuning(ROOT);EXPECTED=TUNING['expected_deposits'];GUIDES=len(TUNING['active_blocker_indices'])
ROOMS=json.loads((ROOT/'Config/MainGameRooms.json').read_text()) if (ROOT/'Config/MainGameRooms.json').exists() else {}
LINKED=ROOMS.get('installed',False)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
previous_lazy=unreal.SystemLibrary.get_console_variable_int_value('bm.Mining.LazyRuntimeChunks')
report={'passed':False,'checks':[],'regions':[]};state={'busy':False,'phase':'setup','start':time.monotonic(),'next':0,'index':0}
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4'];TAGS=['BM_MainGame_V1','BM_MainGame_CavernV2','BM_MainGame_StalagmiteV3','BM_MainGame_NeuralV4']
def V(p):return unreal.Vector(*p)
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def trace(a,b,ignore):
    h=unreal.SystemLibrary.line_trace_single(state['world'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def finish(error=None):
    unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'bm.Mining.LazyRuntimeChunks %d'%previous_lazy)
    report.update(passed=error is None,elapsed=time.monotonic()-state['start'])
    if error:report.update(error=error,phase=state['phase'])
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('ROUTES_RUNTIME '+json.dumps(report,ensure_ascii=False));state['phase']='done';unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def move(p,target):
    state['pawn'].set_actor_location(p-unreal.Vector(0,0,64),False,True);state['movement'].stop_movement_immediately();state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    camera=state['pawn'].get_component_by_class(unreal.CameraComponent);state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(),target))
def tick(dt):
    if state['busy'] or state['phase']=='done':return
    state['busy']=True
    try:
        now=time.monotonic()
        assert now-state['start']<240,'Route verification timed out'
        if now<state['next']:return
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui:return
            state.update(world=world,pawn=pawn,pc=pc,hall=halls[0],movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)))
            check(all(c.get_editor_property('hidden_in_game') for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent)),'Player bodies remain hidden')
            check(len(halls[0].treasure.items)==EXPECTED,'All %d mineral deposits load from the saved map'%EXPECTED)
            state['interaction_targets']=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningTreasure))
            ignored=[pawn]+state['interaction_targets']+[g for r in state['regions'] for g in r.generated_guide_layers]
            for n in range(0 if LINKED else 3):
                floors=[]
                for k in range(61):
                    x=6110+(8750-6110)*k/60;z=900-(x-6500)*.10
                    p=unreal.Vector(x+14800*n,0,z-5680*n)
                    hit=trace(p+unreal.Vector(0,0,200),p-unreal.Vector(0,0,1600),ignored)
                    terrain=[obj for r in state['regions'] for obj in [r.generated_rock,r.generated_shell]]
                    assert hit and hit[0] and hit[9] in terrain,('Connector floor missing',n,k,str(hit[9] if hit else None))
                    floors.append(hit[5].z)
                check(max(abs(b-a) for a,b in zip(floors,floors[1:]))<65,'Connector '+str(n+1)+' retains a continuous floor across both fixed seams')
            state.update(phase='region',next=now+.2)
        elif phase=='region':
            i=state['index'];stage=STAGES[i];r=next(r for r in state['regions'] if r.actor_has_tag(TAGS[i]));origin=r.get_actor_location()+r.cutters[0].local_transform.translation
            paths=json.loads((ART/stage/'centerlines.json').read_text());plan=json.loads((ART/stage/'plan_summary.json').read_text());before=json.loads((OUT/'before.json').read_text())['areas'][i]
            area=next(a for a in state['hall'].areas if str(a.tag)=='BM_Campaign_MainGame%02d'%(i+1))
            delta=V(ROOMS['mine_deltas'][i]) if LINKED else unreal.Vector()
            check(area.map_route[0].distance(V(before['start'])+delta)<.01 and min(p.distance(V(before['end'])+delta) for p in area.map_route)<.01,stage+' local route endpoints preserved after translation')
            entry=V(ROOMS['mine_origins'][i])+unreal.Vector(-6600,0,6096) if LINKED and i else V(before['entry'])+delta
            if LINKED and 'mine_entry_positions' in ROOMS:entry=V(ROOMS['mine_entry_positions'][i])
            exit=(V(ROOMS['connections'][2*i]['b'])+unreal.Vector(-150,0,96) if i<3 else V(ROOMS['mine_origins'][i])+unreal.Vector(6500,0,800)) if LINKED else V(before['exit'])
            if LINKED and i<3 and 'room_activation_x' in ROOMS:exit.x=ROOMS['room_activation_x'][i]
            check(area.entry.translation.distance(entry)<.01 and area.exit.distance(exit)<.01,stage+' campaign entry and exit match current connection plan')
            check(plan['ascent_m']>15 and plan['spiral_turns']==1 and plan['backtracking_x_m']>25,stage+' ascending, helical and returning route segments exist')
            check(len(r.generated_guide_layers)==GUIDES,stage+' keeps %d mineable blockages'%GUIDES)
            ignored=[state['pawn']]+state['interaction_targets']+[g for x in state['regions'] for g in x.generated_guide_layers]
            samples=0;headroom=[]
            for path in paths:
                for k in range(6,len(path['points'])-6,6):
                    if path['name']=='Main' and (path['arc'][k]<12 or path['arc'][k]>path['arc'][-1]-15):continue
                    a=origin+raw(path['points'][k]);b=origin+raw(path['points'][min(k+6,len(path['points'])-1)])
                    h=unreal.SystemLibrary.capsule_trace_single(world,a,b,48,95,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True)
                    h=h.to_tuple() if h else None
                    assert not h or not h[0],(stage,path['name'],k,'Capsule corridor obstructed',str(h[9]))
                    floor=trace(a,a-unreal.Vector(0,0,2200),ignored);ceiling=trace(a,a+unreal.Vector(0,0,2200),ignored)
                    assert floor and floor[0] and ceiling and ceiling[0],(stage,path['name'],k,'Missing enclosing floor/ceiling')
                    assert floor[9]==r.generated_rock and ceiling[9]==r.generated_rock,(stage,path['name'],k,'Wrong enclosing terrain',floor[9].get_actor_label(),ceiling[9].get_actor_label())
                    headroom.append(ceiling[5].z-floor[5].z);samples+=1
            check(min(headroom)>195,stage+' continuous capsule passage with enclosing floor and roof at '+str(samples)+' samples')
            filename={1:'pillars.json',2:'stalagmites.json',3:'neural_rocks.json'}.get(i);landmarks=0
            specs=[]
            if filename:
                specs=json.loads((ART/stage/filename).read_text())
                for spec in specs:
                    p=origin+raw(spec.get('probe_m',spec['center_m']));h=trace(p+unreal.Vector(210,0,0),p,ignored)
                    assert h and h[0] and h[9]==r.generated_rock,(stage,spec['name'],'Missing physical landmark')
                    landmarks+=1
                check(landmarks=={1:12,2:20,3:8}[i],stage+' landscape count and mineable physical collision validated')
            center=plan['cavern_centers_arc_m'][1];idx=min(range(len(paths[0]['arc'])),key=lambda k:abs(paths[0]['arc'][k]-center));p=origin+raw(paths[0]['points'][idx]);target=origin+raw(paths[0]['points'][min(idx+35,len(paths[0]['points'])-1)])
            photo_floor=trace(p,p-unreal.Vector(0,0,2200),ignored)
            if specs:
                subject=next(s for s in specs if s['cavern_index']==1)
                target=origin+raw(subject.get('probe_m',subject['center_m']))
            move(photo_floor[5]+unreal.Vector(0,0,170),target)
            anchor=photo_floor
            for direction in [unreal.Vector(0,1,0),unreal.Vector(0,-1,0),unreal.Vector(1,0,0),unreal.Vector(-1,0,0)]:
                candidate=trace(p,p+direction*3000,ignored)
                if candidate and candidate[0] and candidate[9]==r.generated_rock:
                    anchor=candidate;break
            assert anchor and anchor[0] and anchor[9]==r.generated_rock
            state.update(region=r,stage_name=stage,initial=r.generated_rock.triangle_count,wall=anchor[5],ignored=ignored,phase='photo',next=now+.7)
            report['regions'].append(dict(stage=stage,capsule_samples=samples,landmarks=landmarks,min_headroom_cm=min(headroom)))
        elif phase=='photo':
            check(helper.capture_preview(state['pc'],str(OUT/(state['stage_name']+'_landscape.png'))),state['stage_name']+' PIE landscape screenshot')
            check(state['region'].generated_rock.mine_blast(state['wall'],110,.08,20300+state['index']),state['stage_name']+' normal terrain accepts excavation')
            state.update(phase='reset',next=now+.25)
        elif phase=='reset':
            rock=state['region'].generated_rock
            if not rock.successful_cuts:return
            rock.reset_rock();check(rock.successful_cuts==0 and rock.triangle_count==state['initial'],state['stage_name']+' reset restores new route terrain and solid clay')
            state['index']+=1
            if state['index']<4:state.update(phase='region',next=now+.2)
            else:finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
# This audit traces all four areas remotely from the bedroom. Load collision
# eagerly in the PIE copy; normal play retains the original lazy streaming.
unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'bm.Mining.LazyRuntimeChunks 0')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
