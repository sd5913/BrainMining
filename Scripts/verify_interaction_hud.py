import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/InteractionHUD'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': []}
state = {'phase': 'setup', 'started': time.monotonic(), 'next': 0, 'busy': False}
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
previous_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)


def check(condition, label):
    assert condition, label
    report['checks'].append(label)


def key(name, pressed):
    helper.send_preview_key(state['pc'], name, pressed)


def screenshot(name):
    check(helper.capture_preview(state['pc'], str(output / (name + '.png'))), 'Captured ' + name)


def finish(passed):
    report.update(passed=passed, elapsed=time.monotonic() - state['started'])
    (output / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('INTERACTION_HUD_RESULT ' + json.dumps(report))
    performance.set_editor_property('bThrottleCPUWhenNotForeground', previous_throttle)
    if 'pc' in state:
        key('E', False)
        key('Tab', False)
    levels.editor_request_end_play()
    state.update(phase='exit', next=time.monotonic() + 2)


def tick(delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'exit':
            if now >= state['next'] and editor.get_game_world() is None:
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
            return
        if now - state['started'] > 180:
            raise RuntimeError('Timeout: ' + state['phase'])
        if now < state['next']:
            return
        phase = state['phase']
        if phase == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property('bReady') or unreal.GameplayStatics.get_time_seconds(world) < 3:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            ui = pc.interaction_hud
            wallet = pawn.get_component_by_class(unreal.MiningProgressionComponent).get_wallet()
            state.update(world=world, pc=pc, pawn=pawn, ui=ui, wallet=wallet, hall=halls[0])
            check(ui is not None and ui.is_in_viewport(), 'Default campaign creates the shared HUD')
            check(ui.exploration_visible and not ui.coin_gain_visible, 'Exploration starts with reticle and no phantom income')
            screenshot('01_crosshair')
            wallet.add_coins(25)
            wallet.add_coins(10)
            state.update(phase='gain', next=now + .12)
        elif phase == 'gain':
            check(state['ui'].coin_gain_visible and state['ui'].displayed_coin_gain == 35, 'Actual wallet credits merge to +35')
            screenshot('02_coin_gain')
            state.update(phase='expired', next=now + 1.5)
        elif phase == 'expired':
            check(not state['ui'].coin_gain_visible, 'Income feedback expires')
            key('Tab', True)
            state.update(phase='menu', next=now + .25)
        elif phase == 'menu':
            check(not state['ui'].exploration_visible and not state['ui'].interaction_action, 'Upgrade menu hides reticle and interaction prompt')
            helper.send_preview_focused_key(state['pc'], 'Tab', False)
            state.update(phase='prepare-target', next=now + .3)
        elif phase == 'prepare-target':
            check(state['ui'].exploration_visible, 'Closing menu restores reticle')
            item = state['hall'].collection.items[0]
            placement = next(entry for entry in state['hall'].collection.authored_placements if entry.item_id == item.item_id)
            rock = placement.host_rock
            check(rock.mine_sphere(item.get_actor_location(), 300), 'Excavate the collectible through real mining')
            state.update(item=item, phase='released', next=now + .6)
        elif phase == 'released':
            item = state['item']
            check(item.get_editor_property('bReleased'), 'Exposed collectible releases through gameplay')
            body = item.get_editor_property('Body')
            body.set_simulate_physics(False)
            body.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
            camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
            location = camera.get_world_location() + camera.get_forward_vector() * 150
            item.set_actor_location(location, False, True)
            state.update(item=item, phase='target', next=now + .5)
        elif phase == 'target':
            check(state['hall'].collection.target == state['item'], 'Real sight trace selects collectible')
            check(state['ui'].interaction_action.startswith('收集'), 'Target shows shared E action')
            screenshot('03_interaction')
            rotation = state['pc'].get_control_rotation()
            rotation.yaw += 90
            state['pc'].set_control_rotation(rotation)
            state.update(phase='look-away', next=now + .3)
        elif phase == 'look-away':
            check(not state['ui'].interaction_action, 'Looking away removes E prompt')
            camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), state['item'].get_actor_location()))
            state.update(phase='collect', next=now + .3)
        elif phase == 'collect':
            check(state['ui'].interaction_action.startswith('收集'), 'Looking back restores E prompt')
            key('E', True)
            state.update(phase='collected', next=now + .25)
        elif phase == 'collected':
            key('E', False)
            check(state['item'].get_editor_property('bCollected'), 'Real E input still collects the selected item')
            check(not state['ui'].interaction_action, 'Collected target clears shared prompt')
            check(not state['ui'].coin_gain_visible, 'Non-currency collectible does not invent income')
            finish(True)
    except Exception:
        report['error'] = traceback.format_exc()
        finish(False)
    finally:
        state['busy'] = False


assert not editor.get_game_world(), 'Do not interrupt an existing preview'
levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
helper.start_floating_preview()
callback = unreal.register_slate_post_tick_callback(tick)
