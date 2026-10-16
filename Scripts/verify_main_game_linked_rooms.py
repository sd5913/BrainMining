"""PIE: source references, all six seams, real office workflow, shared systems.

Room access is unlocked for isolated seam checks. Card/TV gameplay has a separate
native full-flow test; the mirror room runs its native physical solution audit.
All changes below affect only the PIE copy.
"""
import unreal,json,time,traceback,sys,math,re
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRooms';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());assert cfg['installed']
sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as geometry
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
report={'passed':False,'checks':[],'connections':[]};state={'phase':'setup','busy':False,'next':0,'start':time.monotonic(),'pickup_index':0,'link_index':0}
def V(p):return unreal.Vector(*p)
def check(ok,msg):
    assert ok,msg;report['checks'].append(msg);report['phase']=state['phase'];(OUT/'runtime_progress.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def key(k,on):helper.send_preview_key(state['pc'],k,on)
def press(k):key(k,True);key(k,False)
def enter(index):
    # Isolated fixture access; restore campaign rules before the next tick.
    hall=state['hall'];hall.set_editor_property('linear_campaign',False)
    try:return hall.enter_area(index)
    finally:hall.set_editor_property('linear_campaign',True)
def move(p,target=None):
    state['pawn'].set_actor_location(p,False,True);state['movement'].stop_movement_immediately();state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    if target:state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),target))
def shot(name):check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'PIE image '+name)
def trace(a,b,capsule=False,ignore=None):
    ignore=ignore if ignore is not None else [state['pawn']]+list(state['hall'].treasure.items)+state.get('gates',[])
    if capsule:
        c=state['pawn'].get_component_by_class(unreal.CapsuleComponent);h=unreal.SystemLibrary.capsule_trace_single(state['world'],a,b,c.get_scaled_capsule_radius(),c.get_scaled_capsule_half_height(),unreal.TraceTypeQuery.ECC_VISIBILITY,False,ignore,unreal.DrawDebugTrace.NONE,True)
    else:h=unreal.SystemLibrary.line_trace_single(state['world'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def finish(error=None):
    if state.get('pc'):
        for k in ['E','G','V','R','W','LeftMouseButton','Tab']:key(k,False)
    if state.get('world'):unreal.GameplayStatics.set_global_time_dilation(state['world'],1)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',throttle);report.update(passed=error is None,elapsed=time.monotonic()-state['start'])
    if error:report.update(error=error,phase=state['phase'])
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('LINKED_ROOMS_RUNTIME '+json.dumps(report,ensure_ascii=False));state['phase']='done';unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
    if state['busy'] or state['phase']=='done':return
    state['busy']=True
    try:
        now=time.monotonic();assert now-state['start']<330,'Linked room audit timed out at '+state['phase']
        if now<state['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall);pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not halls or not halls[0].get_editor_property('bReady') or not pc or not pc.mining_gui:return
            hall=halls[0];room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0]
            state.update(world=world,hall=hall,pc=pc,pawn=pawn,initial_pawn=pawn,room=room,movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),progression=pawn.get_component_by_class(unreal.MiningProgressionComponent),bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent),mining=pawn.get_component_by_class(unreal.MiningComponent),pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),gates=[hall.areas[i].progress_gate for i in [1,3,5]])
            state['wallet']=state['progression'].get_wallet();state['office']=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PopupCloseStage)[0];state['escape']=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningRoomEscape)[0];state['mirror']=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WenSiMirrorPuzzle)[0]
            check(len(hall.areas)==7,'Seven ordered areas: mine/office/mine/journey/mine/museum/mine')
            check(len(hall.treasure.items)==165,'All 165 mineral deposits survive relocation')
            regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion));check(len(regions)==4 and all(len(r.generated_guide_layers)==8 for r in regions),'Four original excavation regions and 32 route plugs; no reserved connector regions')
            check(state['bomb'].is_skill_unlocked(),'Office does not relock MainGame bomb ownership')
            check(all(c.get_editor_property('hidden_in_game') for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent)),'MainGame player body remains hidden')
            for i,a in zip([1,3,5],[state['office'],state['escape'],state['mirror']]):check(a.get_level()==hall.areas[i].source_level.get_loaded_level() and a.get_level()!=hall.get_level(),'Room '+str(i)+' runs from its source sublevel')
            check(all(g.get_actor_enable_collision() for g in state['gates']),'All completion gates begin closed')
            state.update(phase='pickup_place',next=now+.2)
        elif phase=='pickup_place':
            p=state['pickups'][state['pickup_index']].get_actor_location();move(unreal.Vector(p.x-125,p.y,state['room'].get_actor_location().z+102),p);state.update(phase='pickup',next=now+.3)
        elif phase=='pickup':
            press('E');check(state['pickups'][state['pickup_index']].collected,'Real E acquires starter equipment in relocated source room');state['pickup_index']+=1
            state.update(phase='pickup_place' if state['pickup_index']<2 else 'prepare_seam',next=now+.3)
        elif phase=='prepare_seam':
            if 'balance' not in state:
                state['wallet'].add_coins(1000000)
                for i in range(4):check(state['hall'].purchase_upgrade(i,1),'Shared upgrade track '+str(i)+' remains purchasable')
                state['balance']=state['wallet'].balance
            i=state['link_index'];spec=cfg['generated_connections'][i];area_index=spec['mine']*2
            check(enter(area_index),'Enter excavation area for seam '+spec['id'])
            plug=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.get_actor_label()=='LINK_'+spec['id']+'_DiggableSoil')
            move(unreal.Vector(spec['plug_x']-150,spec['a'][1],geometry.floor_at(spec,spec['plug_x']-150)+96),plug.get_actor_location())
            state.update(spec=spec,plug=plug,before=plug.triangle_count,seam_attempts=0,phase='seam_blocked',next=now+.8)
        elif phase=='seam_blocked':
            s=state['spec'];a=unreal.Vector(s['plug_x']-160,s['a'][1],geometry.floor_at(s,s['plug_x']-160)+110);b=unreal.Vector(s['plug_x']+160,s['a'][1],geometry.floor_at(s,s['plug_x']+160)+110);h=trace(a,b,True)
            check(h and h[0] and h[9]==state['plug'],s['id']+' begins physically blocked by mineable soil; actual '+str(h[9].get_actor_label() if h and h[0] else h));shot('%02d_%s_blocked'%(state['link_index']+1,s['id']))
            key('LeftMouseButton',True);state.update(phase='seam_mine',next=now+1.2)
        elif phase=='seam_mine':
            key('LeftMouseButton',False);s=state['spec'];check(state['plug'].successful_cuts>0,s['id']+' accepts real pickaxe input')
            check(state['plug'].mine_blast(state['plug'].get_actor_location(),400,.05,18000+state['link_index']),s['id']+' can be widened with normal terrain booleans');state.update(phase='seam_open',next=now+.35)
        elif phase=='seam_open':
            s=state['spec'];path=[unreal.Vector(s['a'][0]+(s['b'][0]-s['a'][0])*j/24,s['a'][1],0) for j in range(25)]
            for p in path:p.z=geometry.floor_at(s,p.x)+103
            h=next((hit for a,b in zip(path,path[1:]) if (hit:=trace(a,b,True)) and hit[0]),None)
            if h and h[0]:
                check(isinstance(h[9],unreal.DiggableRock) and state['seam_attempts']<20,s['id']+' remaining lip must be mineable, not a hard seam: '+h[9].get_actor_label())
                assert h[9].mine_blast(h[5],135,.05,19000+state['seam_attempts']);state['seam_attempts']+=1;state['next']=now+.25;return
            check(True,s['id']+' full capsule passage opens after excavation')
            for j in range(13):
                p=V(s['a'])+(V(s['b'])-V(s['a']))*(j/12);p.z=geometry.floor_at(s,p.x)
                ignored=[state['pawn']]+list(state['hall'].treasure.items)+state.get('gates',[])+list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock))
                h=trace(p+unreal.Vector(0,0,200),p-unreal.Vector(0,0,300),ignore=ignored)
                depth=s.get('backing_depth',0);assert h and h[0] and -depth-5<h[5].z-p.z<5,(s['id'],'Missing bedrock floor backing',j,str(h[9] if h else None),str(h[5] if h else None))
                surface=trace(p+unreal.Vector(0,0,210),p-unreal.Vector(0,0,120));assert surface and surface[0] and abs(surface[5].z-p.z)<10,(s['id'],'Uneven walking surface',j,str(surface[5] if surface else None))
            check(True,s['id']+' continuous natural rock floor with unmineable backing');shot('%02d_%s_open'%(state['link_index']+1,s['id']))
            state['reset_pos']=state['pawn'].get_actor_location();press('R');state.update(phase='seam_reset',next=now+.5)
        elif phase=='seam_reset':
            check(state['pawn'].get_actor_location().distance(state['reset_pos'])<30,'R stays in the excavation area; echo does not steal restart')
            check(state['plug'].successful_cuts==0 and state['plug'].triangle_count==state['before'],state['spec']['id']+' R restores the seam soil')
            check(state['wallet'].balance==state['balance'] and all(state['progression'].get_level(i)==1 for i in range(4)),'Mining reset preserves wallet and all four upgrades')
            report['connections'].append(dict(id=state['spec']['id'],lip_cuts=state['seam_attempts']));state['link_index']+=1
            state.update(phase='prepare_seam' if state['link_index']<6 else 'office_start',next=now+.1)
        elif phase=='office_start':
            check(enter(1),'Enter the referenced office');o=V(cfg['rooms'][0]['origin']);move(o+unreal.Vector(-190,0,98),o+unreal.Vector(0,0,124));state.update(phase='office_interact',next=now+.8)
        elif phase=='office_interact':
            press('E');state.update(phase='office_validate',next=now+15)
        elif phase=='office_validate':
            result=state['office'].validate_office();check(result.startswith('PASS'),'Office native computer/printer/coin/input audit: '+result);report['office_validation']=result
            unreal.GameplayStatics.set_global_time_dilation(world,4);state.update(phase='office_wait',office_wait=now,next=now+.5)
        elif phase=='office_wait':
            status=state['office'].get_exit_status()
            if 'Exit phase=2' not in status:
                assert now-state['office_wait']<90,'Office finale did not reach bomb pickup: '+status;state['next']=now+.5;return
            unreal.GameplayStatics.set_global_time_dilation(world,1)
            if 'pickup_settle' not in state:state['pickup_settle']=now;state['next']=now+2.2;return
            shot('07_office_eruption_and_bomb')
            pickup=next(c for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeBombSkillPickup');p=pickup.get_attach_parent().get_world_location();o=V(cfg['rooms'][0]['origin']);move(unreal.Vector(p.x-110,p.y,o.z+98),p);state.update(phase='office_pick_bomb',next=now+.4)
        elif phase=='office_pick_bomb':
            key('E',True);state.update(phase='office_pick_result',next=now+.15)
        elif phase=='office_pick_result':
            key('E',False);check('Exit phase=3' in state['office'].get_exit_status(),'Real E collects the office bomb without replacing MainGame abilities');press('G');state.update(phase='office_throw',next=now+.2)
        elif phase=='office_throw':
            press('LeftMouseButton');state.update(phase='office_place_bomb',next=now+.2)
        elif phase=='office_place_bomb':
            bomb=state['bomb'].last_thrown_bomb;check(bool(bomb),'Shared MainGame bomb is thrown in the office');bomb.set_actor_location(V(cfg['rooms'][0]['origin'])+unreal.Vector(968,0,85),False,True);bomb.get_component_by_class(unreal.PrimitiveComponent).set_physics_linear_velocity(unreal.Vector());state.update(phase='office_breach',next=now+3.6)
        elif phase=='office_breach':
            check('door_open=1' in state['office'].get_exit_status(),'Real shared bomb explosion breaches the authored office door')
            press('G');move(V(cfg['rooms'][0]['origin'])+unreal.Vector(1135,0,98));state.update(phase='office_escape',next=now+.3)
        elif phase=='office_escape':
            check(state['office'].is_office_escaped(),'Office evacuation is completed physically before leaving');state['balance']=state['wallet'].balance
            move(V(cfg['rooms'][0]['exit'])+unreal.Vector(60,0,0));state.update(phase='office_to_mine',next=now+.7)
        elif phase=='office_to_mine':
            check(state['hall'].active_area==2,'Completed office leads continuously into excavation area two')
            check(state['wallet'].balance==state['balance'] and unreal.GameplayStatics.get_player_pawn(world,0)==state['initial_pawn'],'Office earnings and the same pawn persist into mining');check(state['bomb'].is_skill_unlocked(),'Bomb ownership persists after the office')
            check(enter(5),'Enter the referenced mirror museum');state.update(phase='mirror_validate',next=now+.7)
        elif phase=='mirror_validate':
            result=state['mirror'].validate_mirror_puzzle();check(result.startswith('PASS'),'Native physical mirror puzzle solutions pass in relocated source room: '+result);report['mirror_validation']=result
            check(state['hall'].skip_current_room(),'Existing F7 room skip remains available after puzzle audit');state.update(phase='final_mine',next=now+.6)
        elif phase=='final_mine':
            check(state['hall'].active_area==6 and unreal.GameplayStatics.get_player_pawn(world,0)==state['initial_pawn'],'Museum exit restores the same MainGame pawn in the fourth mine')
            check(state['wallet'].balance==state['balance'] and all(state['progression'].get_level(i)==1 for i in range(4)),'All shared upgrades and coins survive room transitions')
            check(not state['hall'].campaign_complete,'Entering the last mine does not prematurely complete MainGame')
            # End-signal contract; the native card/TV test exercises the actual
            # card win, rising water, checkpoint retry and all fifteen cuts.
            state['reset_pos']=state['pawn'].get_actor_location();press('R');state.update(phase='post_journey_reset',next=now+.4)
        elif phase=='post_journey_reset':
            check(state['pawn'].get_actor_location().distance(state['reset_pos'])<30,'Inactive television escape cannot hijack R in the fourth mine')
            move(state['hall'].areas[6].exit);state.update(phase='complete',next=now+.6)
        elif phase=='complete':
            check(state['hall'].campaign_complete,'Reaching the final excavation exit completes the seven-section route');finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
