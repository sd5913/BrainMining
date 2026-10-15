"""PIE check of inherited StartRoom pickups, real shaft traversal and MainGame economy.

Starts from saved MainGame. Changes only the PIE copy and restores editor throttling.
"""
import json,time,traceback,math
from pathlib import Path
import unreal
import sys
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/'Scripts'))
from main_game_route_sources import tuning

ROOT=Path(unreal.Paths.project_dir()).resolve()
EXPECTED=tuning(ROOT)['expected_deposits']
OUT=ROOT/'Saved/Validation/MainGameStartRoom';OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((OUT/'integration.json').read_text(encoding='utf-8'))
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
previous_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
report={'passed':False,'checks':[]};state={'phase':'setup','started':time.monotonic(),'next':0,'busy':False}
def V(p):return unreal.Vector(*p)
def check(ok,message):
    assert ok,message
    report['checks'].append(message)
def key(name,down):helper.send_preview_key(state['pc'],name,down)
def press(name):key(name,True);key(name,False)

def trace(a,b,capsule=False):
    offset=V(manifest.get('junction_world_offset',[0,0,0]));a=list((V(a)+offset).to_tuple());b=list((V(b)+offset).to_tuple())
    if capsule:
        c=state['pawn'].get_component_by_class(unreal.CapsuleComponent)
        hit=unreal.SystemLibrary.capsule_trace_single(state['world'],V(a),V(b),c.get_scaled_capsule_radius(),c.get_scaled_capsule_half_height(),unreal.TraceTypeQuery.ECC_VISIBILITY,False,[state['pawn']],unreal.DrawDebugTrace.NONE,True)
    else:
        ignored=[state['pawn']]
        if state.get('checking_shell'):
            ignored += list(unreal.GameplayStatics.get_all_actors_with_tag(state['world'],'BM_MainGame_3DDirtJunction'))
        hit=unreal.SystemLibrary.line_trace_single(state['world'],V(a),V(b),unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True)
    return hit.to_tuple() if hit else None

def check_junction_shell():
    state['checking_shell']=True
    samples=0
    width=manifest.get('junction_half_width',150);floor=manifest.get('junction_floor_z',6000);roof=manifest.get('junction_roof_z',6330);mid=(floor+roof)/2
    for y in [-2800,-1500,-width-150,0,width+150,1500,1900]:
        for z in [0,3500,floor-50,mid,roof+100,7200,7400]:
            if y==0 and z==mid:continue
            h=trace((-6520,y,z),(-6300,y,z))
            assert h and h[0] and h[9] and not isinstance(h[9],unreal.DiggableRock),('Junction shell gap',y,z)
            samples+=1
    for end in [(-6410,width+100,mid),(-6410,-width-100,mid),(-6410,0,roof+100),(-6410,0,floor-100)]:
        h=trace((-6410,0,mid),end)
        assert h and h[0] and not isinstance(h[9],unreal.DiggableRock),('Open inner edge',end)
    report['junction_shell_samples']=samples+4
    state['checking_shell']=False
    check(True,'Excavated connection retains unmineable sides, roof, floor and full mine-mouth collar')
def shot(name):check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'Captured '+name)
def move(pos,yaw=0):
    state['pawn'].set_actor_location(pos,False,True)
    state['flight'].stop_movement_immediately()
    state['pc'].set_control_rotation(unreal.Rotator(yaw=yaw))
def aim(pickup):
    target=pickup.get_actor_location()
    move(unreal.Vector(target.x-125,target.y,state['offset'].z+102))
    camera=state['pawn'].get_component_by_class(unreal.CameraComponent)
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(),target))
def finish(error=None):
    if state.get('pc'):
        for k in ['W','SpaceBar','LeftMouseButton','Tab']:key(k,False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',previous_throttle)
    report.update(passed=error is None,elapsed_seconds=time.monotonic()-state['started'])
    if error:report.update(error=error,phase=state['phase'],position=list(state['pawn'].get_actor_location().to_tuple()) if state.get('pawn') else None)
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_STARTROOM_RUNTIME '+json.dumps(report,ensure_ascii=False))
    state['phase']='done'
    unreal.unregister_slate_post_tick_callback(callback)
    levels.editor_request_end_play()
def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done':return
        if now-state['started']>160:raise AssertionError('Timed out at '+state['phase'])
        if now<state['next']:return
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            rooms=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)
            if not halls or not halls[0].get_editor_property('bReady') or not rooms:return
            pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not pc.mining_gui:return
            flight=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            pickups=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)
            rock=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.actor_has_tag('BM_StartRoom_ExitBlocker'))
            state.update(world=world,pc=pc,pawn=pawn,hall=halls[0],room=rooms[0],rock=rock,flight=flight,offset=V(manifest['source_offset']),
                pick=next(p for p in pickups if p.equipment==unreal.MiningStarterEquipment.PICKAXE),
                jet=next(p for p in pickups if p.equipment==unreal.MiningStarterEquipment.JETPACK),
                mining=pawn.get_component_by_class(unreal.MiningComponent),progression=pawn.get_component_by_class(unreal.MiningProgressionComponent),
                effects=pawn.get_component_by_class(unreal.MiningToolEffectsComponent))
            state['wallet']=state['progression'].get_wallet()
            clay=list(unreal.GameplayStatics.get_all_actors_with_tag(world,'BM_MainGame_3DDirtJunction'))
            if clay:
                state['clay']=clay[0];state['clay_initial_triangles']=clay[0].triangle_count
            state['plug']=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.get_actor_label()=='STARTROOM_Junction_DiggableBlockage')
            shell=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor) if a.get_actor_label().startswith('STARTROOM_Junction_Bedrock_')]
            check(len(shell)==4,'Four unmineable collar pieces seal the original mine mouth')
            check(manifest['rise_cm']==800,'Vertical shaft is shortened from sixteen to eight metres')
            check(state['plug'].actor_has_tag('BM_Campaign_MainGame01'),'Junction blockage belongs to region-one reset scope')
            h=trace((-6620,0,6100),(-6200,0,6100),True)
            check(h and h[0] and h[9]==state['plug'],'Initial junction blockage completely blocks the player capsule')
            check(pawn.get_class().get_name()=='BP_MainGameCharacter_C','MainGame character and controller remain in use')
            check(pawn.get_actor_location().distance(V(manifest['bedroom_spawn']))<70,'MainGame starts inside the inherited bedroom')
            check(not state['room'].pickaxe_unlocked and not state['room'].jetpack_unlocked,'Both starter tools begin locked')
            check(not state['mining'].try_mine(),'Mining API respects the introductory lock')
            check(state['effects'].pickaxe_mesh and not state['effects'].pickaxe_mesh.is_visible(),'Tool presentation initializes but remains hidden before acquisition')
            check(len(halls[0].treasure.items)==EXPECTED,'All %d mineral deposits remain loaded'%EXPECTED)
            check(rock.actor_has_tag('BM_Campaign_MainGame01'),'Inherited diggable door belongs to region one reset scope')
            cap=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_name().split('_')[:1]==['StaticMeshActor'] and a.get_actor_label()=='FutureConnectionCap')
            check(not cap.get_actor_enable_collision() and cap.get_editor_property('hidden'),'Source end cap is disabled only in MainGame')
            exposure=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PostProcessVolume) if a.get_actor_label()=='StartRoom_Exposure')
            check(not exposure.get_editor_property('enabled'),'Source room exposure does not override the mining level globally')
            press('G');press('V');press('Tab')
            check(not state['mining'].drill_mode and not pc.upgrade_menu_held,'Alternate tools cannot bypass the introductory pickups')
            shot('01_inherited_bedroom')
            key('SpaceBar',True);state.update(phase='locked_flight',next=now+.3)
        elif phase=='locked_flight':
            check(not state['flight'].flight_input_held and state['flight'].jetpack_heat==0,'Locked backpack cannot generate flight or heat')
            key('SpaceBar',False);aim(state['jet']);state.update(phase='jet',next=now+.3)
        elif phase=='jet':
            check(state['room'].get_target(state['pc'])==state['jet'],'Transformed backpack is aimable at its visible model')
            check('喷气背包' in state['pc'].interaction_hud.interaction_action,'Existing HUD displays backpack pickup action')
            press('E')
            check(state['room'].jetpack_unlocked and state['jet'].collected,'Real E picks up the inherited jetpack')
            check(not state['room'].pickaxe_unlocked and not state['mining'].try_mine(),'Backpack does not unlock the pickaxe')
            aim(state['pick']);state.update(phase='pick',next=now+.3)
        elif phase=='pick':
            check(state['room'].get_target(state['pc'])==state['pick'],'Transformed pickaxe is independently aimable')
            press('E')
            check(state['room'].pickaxe_unlocked and state['pick'].collected,'Real E picks up the inherited pickaxe')
            check(not state['room'].interact(state['pc']),'Equipment cannot be collected twice')
            key('Tab',True);state.update(phase='menu',next=now+.3)
        elif phase=='menu':
            check(state['pc'].upgrade_menu_held,'MainGame upgrade panel resumes after both pickups')
            key('Tab',False);move(state['offset']+unreal.Vector(350,0,102))
            state['cuts_before']=state['rock'].successful_cuts
            key('LeftMouseButton',True);state.update(phase='mine',next=now+3.5)
        elif phase=='mine':
            key('LeftMouseButton',False)
            check(state['effects'].pickaxe_mesh.is_visible(),'Acquired pickaxe becomes visible with its existing presentation')
            check(state['rock'].successful_cuts>state['cuts_before'],'Real pickaxe input excavates the inherited bedroom exit')
            check(state['effects'].swing_count>0,'Pickaxe swing delegates remain functional after acquisition')
            # Widen the test opening with the same terrain boolean API; no hidden/collision bypass.
            state['rock'].mine_blast(state['rock'].get_actor_location(),210,.1,7201)
            shot('02_opened_bedroom_exit')
            key('W',True);state.update(phase='walk_to_shaft',next=now+.1)
        elif phase=='walk_to_shaft':
            p=state['pawn'].get_actor_location();base=V(manifest['shaft_base'])
            if p.x>=base.x-45:
                key('W',False);state['flight'].stop_movement_immediately()
                check(p.x>state['offset'].x+1200,'Player walks through the excavated exit and lower passage with capsule collision')
                state['base_z']=p.z
                # Temporarily disable thrust, retaining real jump/gravity, to verify the vertical gate.
                settings=state['flight'].flight_settings;state['flight_settings']=settings.copy()
                settings.set_editor_property('enabled',False);state['flight'].set_editor_property('flight_settings',settings)
                key('SpaceBar',True);state.update(phase='jump_only',next=now+1)
            elif now-state['started']>55:raise AssertionError('Cannot walk from bedroom to shaft')
        elif phase=='jump_only':
            key('SpaceBar',False)
            check(state['pawn'].get_actor_location().z<state['base_z']+400,'Normal jump cannot reach the eight metre landing')
            state['flight'].set_editor_property('flight_settings',state['flight_settings'])
            state.update(phase='settle',next=now+1.5)
        elif phase=='settle':
            check(state['flight'].jetpack_heat<.01,'Jetpack naturally starts the ascent fully cooled')
            check(all(state['progression'].get_level(i)==0 for i in range(4)),'Shaft is tested with no ability upgrades')
            state['pc'].set_control_rotation(unreal.Rotator(pitch=68,yaw=0))
            state.update(phase='shaft_preview',next=now+.25)
        elif phase=='shaft_preview':
            shot('03_vertical_shaft')
            state['pc'].set_control_rotation(unreal.Rotator(yaw=0))
            state['ascent_start']=now;state['ascent_game_start']=unreal.GameplayStatics.get_time_seconds(world);key('SpaceBar',True)
            state.update(phase='ascend',next=now+.05)
        elif phase=='ascend':
            p=state['pawn'].get_actor_location();landing=V(manifest['shaft_landing'])
            report['maximum_ascent_heat']=max(report.get('maximum_ascent_heat',0),state['flight'].jetpack_heat)
            if p.z>=landing.z+118:
                check(state['flight'].jetpack_heat<1 and state['flight'].velocity.z>0,'Unupgraded jetpack clears the shaft before overheating')
                report['ascent_seconds']=unreal.GameplayStatics.get_time_seconds(world)-state['ascent_game_start']
                shot('04_jetpack_landing')
                key('W',True);state.update(phase='land',next=now+.05)
            elif unreal.GameplayStatics.get_time_seconds(world)-state['ascent_game_start']>4.7:raise AssertionError('Jetpack could not reach landing within its heat budget')
        elif phase=='land':
            report['maximum_ascent_heat']=max(report.get('maximum_ascent_heat',0),state['flight'].jetpack_heat)
            if state['pawn'].get_actor_location().x>V(manifest['shaft_base']).x+260:
                key('SpaceBar',False)
                check(state['flight'].jetpack_heat<1,'Player flies out of the shaft onto its upper passage before overheating')
                state.update(phase='upper_walk',next=now+.1)
            elif unreal.GameplayStatics.get_time_seconds(world)-state['ascent_game_start']>4.95:raise AssertionError('Cannot leave shaft onto the upper landing')
        elif phase=='upper_walk':
            p=state['pawn'].get_actor_location()
            if not state.get('junction_opened') and p.x>-6550+manifest.get('junction_world_offset',[0,0,0])[0]:
                key('W',False);state['flight'].stop_movement_immediately()
                state['plug_cuts']=state['plug'].successful_cuts
                shot('05_junction_blocked')
                key('W',True);state.update(phase='blocked_walk',next=now+.6)
                return
            if p.x>manifest['original_mine_entry'][0]-30:
                key('W',False);state['flight'].stop_movement_immediately()
                check(p.z>manifest['original_mine_entry'][2]-150,'Player walks into region one without falling out of the world')
                check(state['hall'].active_area==0,'Bedroom, shaft and mine share the first mining area')
                check(state['room'].pickaxe_unlocked and state['room'].jetpack_unlocked,'Acquired abilities persist into MainGame mining')
                shot('05_entered_region_one')
                state['wallet'].add_coins(50000)
                check(state['hall'].purchase_upgrade(1,1),'MainGame jetpack upgrade remains purchasable')
                check(state['hall'].purchase_upgrade(2,1),'MainGame bomb upgrade remains purchasable')
                check(state['hall'].purchase_upgrade(3,1),'MainGame drill upgrade remains purchasable')
                press('V');state.update(phase='tools',next=now+.25)
            elif now-state['ascent_start']>55:raise AssertionError('Upper passage fails to connect to the existing mining entrance')
        elif phase=='blocked_walk':
            key('W',False)
            check(state['pawn'].get_actor_location().x<-6460+manifest.get('junction_world_offset',[0,0,0])[0],'Walking alone cannot cross the mine connection blockage')
            key('LeftMouseButton',True);state.update(phase='mine_junction',next=now+4)
        elif phase=='mine_junction':
            key('LeftMouseButton',False)
            check(state['plug'].successful_cuts>state['plug_cuts'],'Real pickaxe input excavates the new mine-connection blockage')
            state['plug'].mine_blast(state['plug'].get_actor_location(),480 if 'junction_half_width' in manifest else 300,.05,7401)
            state.update(phase='clear_junction',next=now+.25)
        elif phase=='clear_junction':
            h=trace((-6620,0,6100),(-6190,0,6100),True)
            check(not h or not h[0],'Excavating the blockage opens the complete player capsule path')
            if 'junction_half_width' in manifest:
                for y in [-240,-120,0,120,240]:
                    h=trace((-6800,y,6110),(-6270,y,6110),True)
                    if h and h[0]:
                        check(isinstance(h[9],unreal.DiggableRock) and state.get('wide_lip_cuts',0)<16,'Any remaining natural rock lip is diggable, not fixed pipe or bedrock')
                        check(h[9].mine_blast(h[5],135,.05,7600+state.get('wide_lip_cuts',0)),'Pickaxe-scale excavation widens the ordinary rock lip at '+str(y)+'cm')
                        state['wide_lip_cuts']=state.get('wide_lip_cuts',0)+1;state['next']=now+.25;return
                    check(not h or not h[0],'Wide mouth capsule passage remains clear at lateral offset '+str(y)+'cm'+(' hit '+h[9].get_actor_label()+' '+str(h[5]) if h and h[0] and h[9] else ''))
            check_junction_shell()
            shot('06_junction_opened')
            state['junction_opened']=True;key('W',True)
            state.update(phase='upper_walk',next=now+.1)
        elif phase=='tools':
            check(state['mining'].drill_mode,'Real V equips the existing drill after the introduction')
            check(state['effects'].drill_mesh and state['effects'].drill_mesh.is_visible(),'Existing drill presentation remains functional after starter locks')
            press('V');press('G');state.update(phase='bomb',next=now+.25)
        elif phase=='bomb':
            bomb=state['pawn'].get_component_by_class(unreal.MiningBombToolComponent)
            check(bomb.equipped,'Real G equips the existing bomb after the introduction')
            press('G');state['balance']=state['wallet'].balance
            if state.get('clay'):
                check(state['clay'].mine_blast(state['clay'].get_actor_location(),500,.05,9401),'Entrance 3D clay coating can be completely excavated')
            press('R');state.update(phase='reset',next=now+.5)
        elif phase=='reset':
            check(state['room'].pickaxe_unlocked and state['room'].jetpack_unlocked,'R preserves starter equipment acquisition')
            check(state['pick'].collected and state['jet'].collected,'R does not respawn equipment pickups')
            check(state['wallet'].balance==state['balance'] and all(state['progression'].get_level(i)==1 for i in [1,2,3]),'R preserves MainGame wallet and upgrades')
            check(state['rock'].successful_cuts==0,'Region-one reset restores the inherited diggable room exit')
            if state.get('clay'):
                check(state['clay'].successful_cuts==0 and state['clay'].triangle_count==state['clay_initial_triangles'],'Real R restores all entrance 3D clay clods')
            check(state['plug'].successful_cuts==0 and state['plug'].triangle_count>0,'R restores the new junction blockage')
            h=trace((-6620,0,6100),(-6200,0,6100),True)
            check(h and h[0] and h[9]==state['plug'],'Restored junction blockage blocks the capsule again')
            if 'junction_half_width' in manifest:
                for y in [-240,-120,0,120,240]:
                    h=trace((-6620,y,6110),(-6200,y,6110),True)
                    check(h and h[0] and h[9]==state['plug'],'R reseals the wide soil mouth at lateral offset '+str(y)+'cm')
            check(len(state['hall'].treasure.items)==EXPECTED,'Integration keeps all mineral deposits after play')
            move(state['hall'].areas[1].entry.translation)
            state.update(phase='next_region',next=now+1.2)
        elif phase=='next_region':
            check(state['hall'].active_area==1,'Continuous transition into the next section still works')
            check(state['room'].pickaxe_unlocked and state['room'].jetpack_unlocked,'Leaving the bedroom region keeps starter tools unlocked')
            check(state['wallet'].balance==state['balance'] and all(state['progression'].get_level(i)==1 for i in [1,2,3]),'MainGame coins and upgrades persist after leaving the starter region')
            move(state['hall'].areas[0].exit-unreal.Vector(300,0,0))
            state.update(phase='return_region',next=now+1.2)
        elif phase=='return_region':
            check(state['hall'].active_area==0,'Original continuous return into region one still works')
            check(state['room'].pickaxe_unlocked and state['room'].jetpack_unlocked and state['pick'].collected and state['jet'].collected,'Returning to region one does not relock or respawn starter equipment')
            finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False

assert not editor.get_game_world(),'Start with PIE stopped.'
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
