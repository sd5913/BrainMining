import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/TreasureDrop'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': []}
state = {'phase': 'setup', 'started': time.monotonic(), 'next': 0, 'busy': False}

def check(condition, message):
    assert condition, message
    report['checks'].append(message)

def key(name):
    helper.send_preview_key(state['pc'], name, True)
    helper.send_preview_key(state['pc'], name, False)

def capture(name):
    check(helper.capture_preview(state['pc'], str(output / (name+'.png'))), 'Captured '+name)

def aim():
    camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), state['item'].get_actor_location()))

def finish(passed):
    report.update(passed=passed, elapsed=time.monotonic()-state['started'])
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('TREASURE_DROP_RESULT '+json.dumps(report))
    levels.editor_request_end_play()
    state.update(phase='exit', next=time.monotonic()+2)

def tick(delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        phase = state['phase']
        if phase == 'exit':
            if now >= state['next'] and editor.get_game_world() is None:
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
            return
        if now-state['started'] > 160:
            raise RuntimeError('Timeout: '+phase)
        if phase == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property('bReady'):
                return
            hall = halls[0]
            check(hall.enter_area(1), 'Enter the main mineral experience')
            field = hall.treasure
            check(len(field.items) == 18, 'All eighteen minerals still generate')
            check(all(not item.get_editor_property('bReleased') and not item.drop_body.is_simulating_physics() for item in field.items), 'Buried minerals remain fixed')
            item = field.items[0]
            position = item.get_actor_location()
            rock = min(field.rocks, key=lambda candidate: (candidate.get_actor_location()-position).length())
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn.set_actor_location(position+unreal.Vector(0, 290, -30), False, True)
            state.update(world=world, hall=hall, field=field, pawn=pawn, pc=pc, item=item, rock=rock,
                         origin=position, value=item.value, tier=item.size_tier,
                         wallet=pawn.get_component_by_class(unreal.MiningProgressionComponent).get_wallet(),
                         phase='buried', next=unreal.GameplayStatics.get_time_seconds(world)+.5)
            aim()
            return
        game_time = unreal.GameplayStatics.get_time_seconds(state['world'])
        if game_time < state['next']:
            return
        item = state['item']
        if phase == 'buried':
            capture('01_buried')
            check(state['rock'].mine_sphere(item.get_actor_location(), 1000), 'Real rock excavation frees the mineral')
            state.update(phase='released', next=game_time+.2)
        elif phase == 'released':
            check(item.get_editor_property('bReleased') and item.drop_body.is_simulating_physics(), 'Exposed mineral becomes a simulated rigid body')
            check(item.exposure == 1, 'Detached mineral remains collectible')
            check(item.value == state['value'] and item.size_tier == state['tier'], 'Dropping preserves size tier and base value')
            capture('02_released')
            state['spin'] = item.get_actor_rotation()
            state['hall'].return_to_lobby()
            check(not item.drop_body.is_simulating_physics(), 'Leaving the region pauses dropped physics')
            state.update(paused=item.get_actor_location(), phase='paused', next=game_time+.6)
        elif phase == 'paused':
            check((item.get_actor_location()-state['paused']).length() < .1, 'Hidden mineral cannot fall or drift in another region')
            state['hall'].enter_area(1)
            check(item.drop_body.is_simulating_physics(), 'Returning resumes the existing dropped mineral')
            state.update(phase='landed', next=game_time+3)
        elif phase == 'landed':
            position = item.get_actor_location()
            report['drop_height_cm'] = state['origin'].z-position.z
            report['resting_speed_cm_s'] = item.drop_body.get_physics_linear_velocity().length()
            check(position.z < state['origin'].z-20, 'Gravity produces a visible downward fall')
            check(position.z > state['field'].get_actor_location().z-10, 'Floor collision prevents falling through the map')
            check(item.drop_body.get_physics_linear_velocity().length() < 10, 'The mineral settles after landing')
            check(str(item.get_actor_rotation()) != str(state['spin']), 'The mineral visibly tumbles')
            aim()
            state.update(phase='landed_shot', next=game_time+.2)
        elif phase == 'landed_shot':
            capture('03_landed')
            state['rock'].reset_rock()
            state.update(phase='reset', next=game_time+.3)
        elif phase == 'reset':
            check(not item.get_editor_property('bReleased') and not item.drop_body.is_simulating_physics(), 'Resetting rock reburies the uncollected mineral')
            check((item.get_actor_location()-state['origin']).length() < .1, 'Reset restores the original buried transform')
            check(item.exposure < .01 and state['wallet'].balance == 0, 'Reset neither exposes nor rewards a mineral')
            check(state['rock'].mine_sphere(item.get_actor_location(), 1000), 'Reset mineral can be mined again')
            state.update(phase='collect', next=game_time+3)
        elif phase == 'collect':
            check(item.get_editor_property('bReleased'), 'Re-excavated mineral drops again')
            aim()
            state.update(phase='pickup', next=game_time+.2)
        elif phase == 'pickup':
            key('E')
            state.update(phase='collected', next=game_time+.3)
        elif phase == 'collected':
            check(item.get_editor_property('bCollected'), 'E picks up the fallen mineral')
            check(not item.drop_body.is_simulating_physics(), 'Pickup removes the active physics body')
            check(state['wallet'].balance == state['value'], 'Pickup grants the unchanged mineral value once')
            key('E')
            state['rock'].reset_rock()
            state.update(phase='done', next=game_time+.4)
        elif phase == 'done':
            check(item.get_editor_property('bCollected') and state['wallet'].balance == state['value'], 'Repeated pickup and rock reset cannot duplicate the reward')
            capture('04_collected')
            finish(True)
    except Exception:
        report['error'] = traceback.format_exc()
        finish(False)
    finally:
        state['busy'] = False

levels.load_level('/Game/Showcase/Maps/Lvl_MainExperiences')
helper.start_floating_preview()
callback = unreal.register_slate_post_tick_callback(tick)
