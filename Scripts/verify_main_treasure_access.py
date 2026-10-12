import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/MainTreasureAccess'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': []}
state = {'phase': 'warmup', 'started': time.monotonic(), 'ready': None}

def check(condition, message):
    assert condition, message
    report['checks'].append(message)

def finish(passed):
    report['passed'] = passed
    report['elapsed'] = time.monotonic() - state['started']
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('MAIN_TREASURE_ACCESS_RESULT ' + json.dumps(report))
    unreal.unregister_slate_post_tick_callback(callback)
    levels.editor_request_end_play()
    state.update(phase='exit', next=time.monotonic() + 2)

def capture(name):
    check(helper.capture_preview(state['controller'], str(output / (name + '.png'))), 'Captured ' + name)

def walk(target):
    pawn = state['pawn']
    current = pawn.get_actor_location()
    offset = unreal.Vector(target.x-current.x, target.y-current.y, 0)
    if offset.length() < 30:
        pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
        return True
    direction = offset / offset.length()
    pawn.add_movement_input(direction, 1, False)
    state['controller'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(current, target))
    return False

def tick(delta):
    try:
        now = time.monotonic()
        if now-state['started'] > 150:
            raise RuntimeError('Timed out during ' + state['phase'])
        phase = state['phase']
        if phase == 'warmup':
            world = editor.get_game_world()
            if not world:
                return
            if state['ready'] is None:
                state['ready'] = now
            if now-state['ready'] < 8:
                return
            hall = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)[0]
            if not hall.get_editor_property('bReady'):
                return
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            field = hall.treasure
            state.update(world=world, hall=hall, pawn=pawn, controller=controller, field=field,
                         mining=pawn.get_component_by_class(unreal.MiningComponent))
            check(hall.enter_area(1), 'Enter collection and mineral area through the normal area entry')
            state.update(phase='entry', next=now+2)
        elif phase == 'entry' and now >= state['next']:
            field = state['field']
            check(len(field.items) == 18, 'All eighteen buried minerals generate')
            check(all(not item.get_editor_property('hidden') for item in field.items), 'Mineral actors are visible in this area')
            capture('01_entry')
            entry = state['pawn'].get_actor_location()
            report['entry'] = [entry.x, entry.y, entry.z]
            location = field.get_actor_location()
            state.update(phase='walk_court', walk_start=now,
                         waypoint=unreal.Vector(location.x+1650, location.y, entry.z))
        elif phase == 'walk_court':
            if now-state['walk_start'] > 12:
                capture('blocked_route')
                raise RuntimeError('Normal walking cannot reach the mineral court from the collection entry')
            if walk(state['waypoint']):
                check(True, 'Walk from the normal entry into the mineral court without teleporting or flying')
                field = state['field']
                center = field.get_actor_location()
                item = max((item for item in field.items if item.get_actor_location().y > center.y), key=lambda item: item.get_actor_location().x)
                position = item.get_actor_location()
                state.update(item=item, phase='walk_row', walk_start=now,
                             waypoint=unreal.Vector(position.x+280, position.y+300, state['pawn'].get_actor_location().z))
        elif phase == 'walk_row':
            if now-state['walk_start'] > 12:
                raise RuntimeError('Cannot walk to the front mineral row')
            if walk(state['waypoint']):
                position = state['item'].get_actor_location()
                state.update(phase='walk_rock', walk_start=now,
                             waypoint=unreal.Vector(position.x, position.y+270, state['pawn'].get_actor_location().z))
        elif phase == 'walk_rock':
            if now-state['walk_start'] > 12:
                raise RuntimeError('Cannot approach the mineral-bearing rock')
            if walk(state['waypoint']):
                check(True, 'Approach a visible mineral-bearing rock using walking collision')
                camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
                state['controller'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), state['item'].get_actor_location()))
                state.update(phase='before_mine', next=now+1)
        elif phase == 'before_mine' and now >= state['next']:
            capture('02_buried_rock')
            check(not state['item'].get_editor_property('bCollected'), 'Mineral starts uncollected')
            state['value'] = state['item'].value
            state['balance'] = state['field'].coins
            helper.send_preview_key(state['controller'], 'LeftMouseButton', True)
            state.update(phase='dig', next=now+2)
        elif phase == 'dig' and now >= state['next']:
            helper.send_preview_key(state['controller'], 'LeftMouseButton', False)
            check(state['item'].exposure > 0.5, 'Real left-mouse digging exposes more than half the mineral')
            capture('03_exposed_mineral')
            helper.send_preview_key(state['controller'], 'E', True)
            helper.send_preview_key(state['controller'], 'E', False)
            state.update(phase='collected', next=now+1)
        elif phase == 'collected' and now >= state['next']:
            check(state['item'].get_editor_property('bCollected'), 'E collects the exposed mineral')
            check(state['field'].coins == state['balance']+state['value'], 'Mineral credits the shared coin balance once')
            capture('04_collected_coins')
            state['hall'].return_to_lobby()
            state['hall'].enter_area(1)
            check(state['item'].get_editor_property('bCollected'), 'Collected state survives leaving and returning')
            check(unreal.GameplayStatics.get_player_character(state['world'], 0) == state['pawn'], 'The same pawn remains throughout the walkthrough')
            finish(True)
    except Exception:
        report['error'] = traceback.format_exc()
        finish(False)

busy = False
def guarded(delta):
    global busy
    if busy:
        return
    busy = True
    try:
        tick(delta)
    finally:
        busy = False

def quit_after(delta):
    if state['phase'] == 'exit' and time.monotonic() >= state['next'] and editor.get_game_world() is None:
        unreal.unregister_slate_post_tick_callback(exit_callback)
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')

levels.load_level('/Game/Showcase/Maps/Lvl_MainExperiences')
helper.start_floating_preview()
callback = unreal.register_slate_post_tick_callback(guarded)
exit_callback = unreal.register_slate_post_tick_callback(quit_after)
