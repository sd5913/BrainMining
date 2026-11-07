"""Capture both logo variants through MainGame and the real pickaxe interaction.

Run with -NoSteam -MiningQA and an isolated -UserDir.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()) / 'Saved/Validation/OpeningLogos'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
state = {'phase': 'load', 'start': time.monotonic(), 'next': 0, 'busy': False}
report = {'passed': False, 'checks': []}


def check(ok, message):
    assert ok, message
    report['checks'].append(message)


def step(phase, delay=1):
    state.update(phase=phase, next=time.monotonic() + delay)


def place(eye, target):
    pawn = state['pc'].get_controlled_pawn()
    camera = pawn.get_component_by_class(unreal.CameraComponent)
    offset = camera.get_world_location() - pawn.get_actor_location()
    pawn.set_actor_location(eye - offset, False, True)
    pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target))


def finish(error=None):
    report.update(passed=error is None, error=error)
    (out / 'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    levels.editor_request_end_play()
    step('cleanup', 3)


def tick(delta):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        phase = state['phase']
        if phase == 'cleanup':
            if editor.get_game_world():
                return
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        assert time.monotonic() - state['start'] < 300, 'Timed out in ' + phase
        if phase == 'load':
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'mining.SaveEnabled 0')
            helper.start_floating_preview()
            step('ready', 8)
            return
        world = editor.get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        state['pc'] = pc
        actors = lambda cls: unreal.GameplayStatics.get_all_actors_of_class(world, cls)
        if phase == 'ready':
            openings = actors(unreal.MiningOpening)
            if not openings:
                return
            opening = openings[0]
            menu = helper.read_preview_object(opening, 'Menu')
            if not menu:
                return
            state.update(opening=opening, room=actors(unreal.MiningStartRoom)[0])
            resource = helper.read_preview_object(menu, 'TitleImage').get_editor_property('brush').resource_object
            check(resource.get_texture_parameter_value('Logo').get_path_name().startswith('/Game/StartRoom/UI/T_OpeningTitle.'), 'Menu uses supplied cream/gold logo')
            check(not opening.get_components_by_class(unreal.DecalComponent), 'Wall title hidden before pickup')
            helper.resize_preview_viewport(pc, 1920, 1080)
            step('menu', 3)
        elif phase == 'menu':
            check(helper.capture_preview(pc, str(out / 'menu.png')), 'Captured menu')
            state['opening'].start_game()
            step('pickup_view', 5)
        elif phase == 'pickup_view':
            check(state['opening'].get_editor_property('bFinished'), 'Opening completes')
            pickup = next(a for a in actors(unreal.MiningStarterPickup) if a.equipment == unreal.MiningStarterEquipment.PICKAXE)
            pos = pickup.get_actor_location()
            state['pickup'] = pickup
            place(pos + unreal.Vector(-100, 0, 60), pos)
            step('pickup')
        elif phase == 'pickup':
            check(state['room'].get_target(pc) == state['pickup'], 'Real pickaxe is targeted')
            check(state['room'].interact(pc), 'Normal pickup interaction succeeds')
            check(state['room'].get_editor_property('bPickaxeUnlocked'), 'Pickaxe unlocked')
            transform = state['room'].get_actor_transform()
            place(transform.transform_location(unreal.Vector(0, -140, 165)),
                  transform.transform_location(unreal.Vector(466, 0, 168)))
            step('wall', 4)
        elif phase == 'wall':
            decals = state['opening'].get_components_by_class(unreal.DecalComponent)
            check(len(decals) == 1, 'Looking at wall after pickup reveals one logo')
            decal = decals[0]
            material = decal.get_decal_material()
            samples = [n for n in unreal.MaterialEditingLibrary.get_material_expressions(material)
                       if isinstance(n, unreal.MaterialExpressionTextureSample)]
            check(len(samples) == 1 and samples[0].texture.get_path_name().startswith('/Game/StartRoom/UI/T_OpeningWallTitle.'), 'Wall uses separate cream/gold logo')
            size = decal.get_editor_property('decal_size')
            check(abs(size.z / size.y - 1040 / 720) < 0.001, 'Wall preserves supplied aspect ratio')
            check(helper.capture_preview(pc, str(out / 'wall.png')), 'Captured wall')
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
