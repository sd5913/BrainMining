"""Exercise actual MainGame PIE input, pickups, material spending, and persistent HUD."""
import unreal
import json
import time
import traceback
from pathlib import Path
ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Validation/MaterialProgression'
OUT.mkdir(parents=True, exist_ok=True)
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
report = {'passed':False, 'checks':[]}
state = {'phase':'wait', 'start':time.monotonic(), 'next':0, 'busy':False}
render = '-nullrhi' not in unreal.SystemLibrary.get_command_line().lower()

def check(value, label):
    assert value, label
    report['checks'].append(label)

def key(name, down): helper.send_preview_key(state['pc'], name, down)
def press(name): key(name, True); key(name, False)
def shot(name):
    if render: check(helper.capture_preview(state['pc'], str(OUT / (name+'.png'))), 'Screenshot '+name)
def finish(error=None):
    report.update(passed=error is None, error=error, rendered=render, phase=state['phase'], elapsed=time.monotonic()-state['start'])
    result=json.dumps(report, ensure_ascii=False, indent=2)
    (OUT/'runtime.json').write_text(result, encoding='utf-8')
    (OUT/('runtime-rendered.json' if render else 'runtime-headless.json')).write_text(result, encoding='utf-8')
    levels.editor_request_end_play(); state.update(phase='done', next=time.monotonic()+2)
def tick(delta):
    if state['busy'] or time.monotonic() < state['next']: return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'done':
            if not ed.get_game_world():
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(ed.get_editor_world(), 'QUIT_EDITOR')
            return
        assert now-state['start'] < 240, 'PIE verification timed out'
        world = ed.get_game_world()
        if not world: return
        if state['phase'] == 'wait':
            pc = unreal.GameplayStatics.get_player_controller(world,0)
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not pc or not unreal.GameplayStatics.get_player_pawn(world,0) or not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui or not pc.mining_gui.get_editor_property('bMineralProgression'): return
            hall = halls[0]; pawn = unreal.GameplayStatics.get_player_pawn(world,0)
            progress = pawn.get_component_by_class(unreal.MiningProgressionComponent)
            wallet = progress.get_wallet()
            check(wallet.uses_mineral_inventory(), 'MainGame selects material economy in PIE')
            check(len(progress.tracks) == 1, 'Only pickaxe upgrades exist')
            state.update(world=world, pc=pc, hall=hall, pawn=pawn, progress=progress, wallet=wallet, phase='pickups', room=unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningStartRoom)[0], pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningStarterPickup)))
            hall.set_actor_tick_enabled(False)
        elif state['phase'] == 'pickups':
            pending = [p for p in state['pickups'] if not p.get_editor_property('bCollected')]
            if pending:
                pickup = pending[0]; pos = pickup.get_actor_location()
                state['pawn'].set_actor_location(unreal.Vector(pos.x-125,pos.y,state['room'].get_actor_location().z+102),False,True)
                movement=state['pawn'].get_component_by_class(unreal.CharacterMovementComponent)
                movement.stop_movement_immediately()
                movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
                state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), pos))
                state.update(phase='collect_starter', starter=pickup, next=now+.6)
            else:
                check(True, 'Tutorial equipment picked up using actual E input')
                state['hall'].enter_area(0)
                state.update(phase='insufficient', next=now+.5)
        elif state['phase'] == 'collect_starter':
            check(state['room'].get_target(state['pc'])==state['starter'], 'Starter equipment is in the actual sight line')
            press('E'); check(state['starter'].get_editor_property('bCollected'), 'E collects starter equipment')
            state.update(phase='pickups', next=now+.2)
        elif state['phase'] == 'insufficient':
            p=state['progress'];w=state['wallet'];gui=state['pc'].mining_gui
            check(gui.get_editor_property('bMineralProgression'), 'Persistent material HUD enabled')
            check(not gui.get_editor_property('bUpgradePanelOpen'), 'Legacy popup remains closed')
            press('Tab');check(p.get_level(0)==0, 'Tab with empty bag cannot upgrade')
            check(not state['pc'].get_editor_property('bUpgradeMenuHeld') and not state['pc'].get_editor_property('bShowMouseCursor'), 'Tab keeps mouse captured')
            shot('01_empty_inventory')
            # Use a real authored mineral and actual targeting/collection code.
            field=state['hall'].treasure
            ore=next(x for x in field.items if str(x.treasure_id)=='2' and x.actor_has_tag('BM_Campaign_MainGame01'))
            camera=state['pawn'].get_component_by_class(unreal.CameraComponent)
            field.set_actor_tick_enabled(False)
            ore.drop_body.set_simulate_physics(False)
            ore.set_actor_location(camera.get_world_location()+camera.get_forward_vector()*150,False,True)
            state.update(ore=ore, phase='collect_ore', next=now+.25)
        elif state['phase'] == 'collect_ore':
            press('E')
            check(state['ore'].get_editor_property('bCollected'), 'Actual E input collects exposed ore')
            copper=unreal.MiningMineral.COPPER
            check(state['wallet'].get_mineral_count(copper)==1, 'One ore credits exactly one inventory unit')
            press('E');check(state['wallet'].get_mineral_count(copper)==1, 'Repeated pickup cannot duplicate inventory')
            state['wallet'].add_coins(99999);check(state['wallet'].balance==0, 'Legacy coin rewards ignored')
            for mineral,amount in [('2',27),('4',20),('6',6),('9',2)]:state['wallet'].add_mineral(mineral,amount)
            state.update(phase='ready', next=now+.3)
        elif state['phase'] == 'ready':
            shot('02_upgrade_ready');key('Tab',True)
            check(state['progress'].get_level(0)==1, 'Tab press purchases one level')
            state.update(phase='hold', next=now+1)
        elif state['phase'] == 'hold':
            check(state['progress'].get_level(0)==1, 'Holding Tab does not buy more levels')
            key('Tab',False)
            for level in (2,3,4):
                press('Tab');check(state['progress'].get_level(0)==level, 'Tab purchases level '+str(level))
            w=state['wallet']
            for kind in (unreal.MiningMineral.COPPER,unreal.MiningMineral.IRON,unreal.MiningMineral.MALACHITE,unreal.MiningMineral.DIAMOND):
                check(w.get_mineral_count(kind)==0, 'Exact material consumption '+str(kind))
            press('Tab');check(state['progress'].get_level(0)==4, 'Tab at maximum level is harmless')
            m=state['pawn'].get_component_by_class(unreal.MiningComponent)
            check(m.pickaxe_balance.destruction_level==4, 'Original pickaxe effects reach runtime')
            for track in (1,2,3):check(not state['progress'].purchase(track,1), 'Removed tool upgrade rejected '+str(track))
            w.add_mineral('2',3)
            state['hall'].set_editor_property('linear_campaign', False)
            try:
                check(state['hall'].enter_area(2), 'Enter the second mine in the PIE test')
            finally:
                state['hall'].set_editor_property('linear_campaign', True)
            check(w.get_mineral_count(unreal.MiningMineral.COPPER)==3 and state['progress'].get_level(0)==4, 'Bag and upgrades survive area transition')
            state['hall'].enter_area(0)
            state.update(phase='finish', next=now+.4)
        elif state['phase'] == 'finish':
            shot('03_max_level');finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy']=False

assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback = unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
