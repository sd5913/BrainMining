"""PIE integration check with real E/Space/mouse input. Only mutates the PIE world."""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_saved_dir())/'Validation/StartRoom'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
result = {'passed':False, 'checks':[]}
state = {'phase':'wait', 'start':time.monotonic(), 'next':0}
performance=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground',False)

def check(ok, label):
    assert ok, label
    result['checks'].append(label)

def key(name, pressed): helper.send_preview_key(state['pc'], name, pressed)
def press(name): key(name,True); key(name,False)

def trace(a,b):
    hit=unreal.SystemLibrary.line_trace_single(state['world'],unreal.Vector(*a),unreal.Vector(*b),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[state['pawn']],unreal.DrawDebugTrace.NONE,True)
    return hit.to_tuple()[9] if hit and hit.to_tuple()[0] else None

def aim_at(pickup, x=205):
    pos=pickup.get_actor_location()
    state['pawn'].set_actor_location(unreal.Vector(x,pos.y,102),False,True)
    state['flight'].velocity=unreal.Vector()
    eye=state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,pos))

def finish(error=None):
    performance.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    for name in ['SpaceBar','LeftMouseButton','W']: key(name,False)
    result.update(passed=error is None,elapsed_seconds=time.monotonic()-state['start'])
    if error: result['error']=error
    (out/'runtime.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('STARTROOM_RUNTIME '+json.dumps(result,ensure_ascii=False))
    state['phase']='done'
    unreal.unregister_slate_post_tick_callback(callback)
    levels.editor_request_end_play()

def tick(dt):
    try:
        now=time.monotonic()
        if now-state['start']>100: raise AssertionError('Timed out at '+state['phase'])
        if now<state['next']: return
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='wait':
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not pawn or not pc:return
            room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0]
            pickups=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)
            pick=next(p for p in pickups if p.equipment==unreal.MiningStarterEquipment.PICKAXE)
            jet=next(p for p in pickups if p.equipment==unreal.MiningStarterEquipment.JETPACK)
            rock=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.actor_has_tag('BM_StartRoom_ExitBlocker'))
            state.update(world=world,pc=pc,pawn=pawn,room=room,pick=pick,jet=jet,rock=rock,
                         mining=pawn.get_component_by_class(unreal.MiningComponent),
                         flight=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),
                         effects=pawn.get_component_by_class(unreal.MiningToolEffectsComponent))
            check(isinstance(pc,unreal.MiningStartRoomController),'Standalone controller is active')
            check(not room.pickaxe_unlocked and not room.jetpack_unlocked,'Both tools start locked')
            check(not state['mining'].try_mine(),'Direct mining cannot bypass initial lock')
            check(not state['effects'].enabled,'Held pickaxe hidden before acquisition')
            check(trace((400,0,120),(660,0,120))==rock,'Only exit is blocked by real diggable rock')
            for a,b in [((-400,70,160),(-550,70,160)),((0,300,160),(0,440,160)),((0,-300,160),(0,-440,160)),((0,0,270),(0,0,390))]:
                check(trace(a,b) is not None,'Bedroom shell blocks an alternate route')
            press('G');press('V');press('Tab')
            check(not state['mining'].drill_mode and not pc.upgrade_menu_held,'Alternate tools and upgrade UI do not bypass intro')
            check(helper.capture_preview(pc,str(out/'01_bedroom.png')),'Captured bedroom at real spawn')
            key('SpaceBar',True)
            state.update(phase='locked_flight',next=now+.25)
        elif phase=='locked_flight':
            check(not state['flight'].flight_input_held and state['flight'].jetpack_heat==0,'Space cannot activate locked jetpack')
            key('SpaceBar',False)
            press('E')
            check(not state['room'].pickaxe_unlocked and not state['room'].jetpack_unlocked,'E at a distance collects neither item')
            # Take the backpack FIRST to exercise independent acquisition order.
            aim_at(state['jet']);state.update(phase='jet_aim',next=now+.25)
        elif phase=='jet_aim':
            check(state['room'].get_target(state['pc'])==state['jet'],'Jetpack requires nearby unobstructed aim')
            check('喷气背包' in state['pc'].interaction_hud.interaction_action,'Shared HUD displays the jetpack action')
            check(helper.capture_preview(state['pc'],str(out/'02_jetpack_prompt.png')),'Captured pickup prompt')
            press('E')
            check(state['room'].jetpack_unlocked and not state['room'].pickaxe_unlocked,'E unlocks only jetpack when acquired first')
            check(state['jet'].collected and not state['room'].get_target(state['pc']),'Collected jetpack disappears and cannot be targeted again')
            check(not state['mining'].try_mine(),'Jetpack acquisition does not unlock mining')
            key('SpaceBar',True);state.update(phase='jet_flight',next=now+.25)
        elif phase=='jet_flight':
            check(state['flight'].flight_input_held and state['flight'].jetpack_heat>0,'Real Space input activates existing thrust and heat')
            check(state['flight'].velocity.z>0,'Unlocked jetpack produces upward movement')
            key('SpaceBar',False)
            aim_at(state['pick']);state.update(phase='pick_aim',next=now+.25)
        elif phase=='pick_aim':
            check(state['room'].get_target(state['pc'])==state['pick'],'Pickaxe can be targeted from the walkable floor')
            check('镐子' in state['pc'].interaction_hud.interaction_action,'Shared HUD displays the pickaxe action')
            press('E')
            check(state['room'].pickaxe_unlocked and state['pick'].collected,'Real E input unlocks mining and removes pickup')
            check(state['effects'].enabled,'Existing pickaxe presentation is restored')
            check(not state['room'].interact(state['pc']),'Repeated interaction is harmless')
            state['pawn'].set_actor_location(unreal.Vector(350,0,102),False,True)
            state['pc'].set_control_rotation(unreal.Rotator(pitch=-8,yaw=0))
            state.update(phase='mine',next=now+.25,cut_start=state['rock'].successful_cuts)
        elif phase=='mine':
            key('LeftMouseButton',True);state.update(phase='mine_check',next=now+1.8)
        elif phase=='mine_check':
            key('LeftMouseButton',False)
            check(state['rock'].successful_cuts>state['cut_start'],'Real mouse input and existing pickaxe excavate the exit')
            # Continue with the actual mining API across the doorway's capsule clearance.
            state.update(phase='clear',next=now+.1,n=0)
        elif phase=='clear':
            # The shared pickaxe now breaks each patch in three strikes. Mine the
            # actual remaining capsule obstruction instead of assuming 18 cuts suffice.
            obstacle=unreal.SystemLibrary.capsule_trace_single(world,unreal.Vector(350,0,102),unreal.Vector(660,0,102),
                35,88,unreal.TraceTypeQuery.ECC_VISIBILITY,False,[state['pawn']],unreal.DrawDebugTrace.NONE,True)
            if obstacle and obstacle.to_tuple()[0]:
                assert state['n']<100,'Pickaxe did not clear the doorway capsule path'
                hit=obstacle.to_tuple()
                assert hit[9]==state['rock'],'Doorway blocked by a non-diggable actor: '+str(hit[9])
                eye=state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()
                target=hit[5]+unreal.Vector(0,0,30)
                state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
                state['mining'].try_mine();state['n']+=1;state['next']=now+.65
            else:
                state['pc'].set_control_rotation(unreal.Rotator(yaw=0))
                check(helper.capture_preview(state['pc'],str(out/'03_exit_excavated.png')),'Captured the excavated doorway')
                # Walk through it with the character's real capsule and collision.
                key('W',True);state.update(phase='walk_exit',next=now+1.3)
        elif phase=='walk_exit':
            key('W',False)
            result['exit_position_cm']=str(state['pawn'].get_actor_location())
            check(state['pawn'].get_actor_location().x>610,'Player walks through mined opening using real capsule collision')
            press('R');state.update(phase='reset',next=now+.4)
        elif phase=='reset':
            check(state['room'].pickaxe_unlocked and state['room'].jetpack_unlocked,'Terrain reset preserves acquired abilities')
            check(state['pick'].collected and state['jet'].collected,'Terrain reset does not duplicate pickups')
            check(trace((400,0,120),(660,0,120))==state['rock'],'Existing reset restores diggable exit layer')
            finish()
    except Exception:
        if 'pc' in state:
            result['debug']={'phase':state['phase'],'flight_settings':str(state['flight'].flight_settings),
                'input_held':state['flight'].flight_input_held,'heat':state['flight'].jetpack_heat,
                'velocity':str(state['flight'].velocity),'movement_mode':str(state['flight'].movement_mode),
                'move_ignored':state['pawn'].is_move_input_ignored()}
            finish(traceback.format_exc())
        else:
            result['error']=traceback.format_exc()
            (out/'runtime.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
            unreal.unregister_slate_post_tick_callback(callback)

assert not editor.get_game_world(), 'Start verification with PIE stopped.'
assert levels.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
