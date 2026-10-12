"""Real key-input equip transitions and shaft stability in the current main map."""
import json, time, traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root = Path(unreal.Paths.project_dir())
out = root / 'Saved/Validation/DrillStylized'
out.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
h = unreal.MiningGUIEditorLibrary
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
report = {'passed': False, 'checks': [], 'transitions': [], 'max_shaft_drift_cm': 0.}
s = {'phase': 'setup', 'started': time.monotonic(), 'busy': False}

def check(value, label):
    assert value, label
    report['checks'].append(label)

def valid(value):
    return value is not None and unreal.SystemLibrary.is_valid(value)

def key(name, pressed):
    h.send_preview_key(s['pc'], name, pressed)

def press(name):
    key(name, True)
    key(name, False)

def pose(component):
    return [str(component.get_editor_property(p)) for p in
            ['relative_location', 'relative_rotation', 'relative_scale3d']]

def motion_offset(motion):
    # Animation is expressed in the Mount frame; compare actual camera-relative drop.
    v = unreal.MathLibrary.transform_direction(motion.get_attach_parent().get_relative_transform(),
                                              motion.get_editor_property('relative_location'))
    return v.z

def transition(name, input_key, motion, visible):
    press(input_key)
    s.update(phase='transition', transition_name=name, motion=motion, model=visible,
             at=unreal.GameplayStatics.get_time_seconds(s['world']), drops=[])

def finish(ok, reason=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() and valid(s.get('pc')):
        for k in ['G', 'V', 'LeftMouseButton']:
            key(k, False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=ok, reason=reason, elapsed=time.monotonic()-s['started'])
    (out / 'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('STYLIZED_DRILL_VERIFIED ' + json.dumps(report))
    if editor.get_game_world():
        levels.editor_request_end_play()
    s.clear()

def tick(dt):
    assert time.monotonic()-s['started'] < 120, 'Validation timed out: ' + s['phase']
    if s['phase'] == 'setup':
        world = editor.get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        if not pc or not pc.mining_gui or not pawn:
            return
        if any(not hall.get_editor_property('bReady') for hall in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)):
            return
        if 'ready' not in s:
            s['ready'] = time.monotonic()
            return
        if time.monotonic()-s['ready'] < 2:
            return
        e = pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
        mining = pawn.get_component_by_class(unreal.MiningComponent)
        bomb = pawn.get_component_by_class(unreal.MiningBombToolComponent)
        components = {c.get_name(): c for c in pawn.get_components_by_class(unreal.SceneComponent)}
        s.update(world=world, pc=pc, pawn=pawn, e=e, mining=mining, bomb=bomb, components=components)
        report['character'] = pawn.get_class().get_path_name()
        check('SharedMiningCharacter' in report['character'], 'Main experiences uses the shared player character')
        check(e.bomb_pivot == components['BombMotion'], 'Bomb animation uses a separate Motion component')
        check(components['BombModel'].get_attach_parent() == components['BombMotion'], 'Bomb model inherits its equip motion')
        mesh = unreal.load_asset('/Game/Drill/Stylized/SM_DrillStylized')
        material = unreal.load_asset('/Game/Drill/Stylized/M_DrillStylized')
        for model in [e.drill_mesh, e.left_drill_mesh]:
            check(model.static_mesh == mesh and model.get_material(0) == material, model.get_name() + ' uses baked PBR drill')
        check(e.get_editor_property('drill_spin_curve_asset').get_path_name() == '/Game/Drill/CF_DrillSpin.CF_DrillSpin', 'Existing authored spin curve is retained')
        check(e.drill_spin_axis == unreal.Vector(1, 0, 0), 'New mesh shaft matches model-local X axis')
        check(all(c.green_segment4 for c in [pc.mining_gui.pickaxe_upgrade, pc.mining_gui.vortex_upgrade, pc.mining_gui.blast_upgrade]), 'All existing four-level upgrade indicators remain present')
        s['poses'] = {n: pose(c) for n, c in components.items() if n.endswith(('Mount', 'Model')) and n.startswith(('Pickaxe', 'Drill', 'Bomb'))}
        # Change duration only in the disposable PIE instance for reliable sampling.
        e.tool_equip_duration = .5
        pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
        transition('pickaxe-to-bomb', 'G', e.bomb_pivot, components['BombModel'])
        return
    now = unreal.GameplayStatics.get_time_seconds(s['world'])
    if s['phase'] == 'transition':
        elapsed = now-s['at']
        drop = motion_offset(s['motion'])
        s['drops'].append([elapsed, drop])
        if elapsed < .65:
            return
        check(min(d[1] for d in s['drops']) < -2., s['transition_name'] + ' starts below the held pose')
        check(abs(drop) < .02, s['transition_name'] + ' eases back to the artist pose')
        check(s['model'].is_visible(), s['transition_name'] + ' displays the selected tool')
        report['transitions'].append({'name': s['transition_name'], 'samples': s['drops']})
        name = s['transition_name']
        if name == 'pickaxe-to-bomb':
            transition('bomb-to-pickaxe', 'G', s['e'].pickaxe_pivot, s['e'].pickaxe_mesh)
        elif name == 'bomb-to-pickaxe':
            transition('pickaxe-to-drills', 'V', s['e'].drill_pivot, s['e'].drill_mesh)
        elif name == 'pickaxe-to-drills':
            s['shaft_targets'] = {}
            s['angles'] = []
            for model in [s['e'].drill_mesh, s['e'].left_drill_mesh]:
                for along in [0., 15.]:
                    s['shaft_targets'][(model.get_name(), along)] = unreal.MathLibrary.transform_location(model.get_relative_transform(), unreal.Vector(along, 0, 0))
            key('LeftMouseButton', True)
            s.update(phase='spin', at=now)
        elif name == 'drills-to-bomb':
            transition('throw-to-pickaxe', 'LeftMouseButton', s['e'].pickaxe_pivot, s['e'].pickaxe_mesh)
        elif name == 'throw-to-pickaxe':
            check(valid(s['bomb'].last_thrown_bomb), 'Left click still throws the sticky bomb')
            check(s['bomb'].settings.fuse_seconds == 3., 'Bomb keeps the original three-second fuse')
            for n, expected in s['poses'].items():
                check(pose(s['components'][n]) == expected, n + ' editable pose survives every animation')
            finish(True)
    elif s['phase'] == 'spin':
        e = s['e']
        for model in [e.drill_mesh, e.left_drill_mesh]:
            for along in [0., 15.]:
                p = unreal.MathLibrary.transform_location(model.get_attach_parent().get_relative_transform(),
                    unreal.MathLibrary.transform_location(model.get_relative_transform(), unreal.Vector(along, 0, 0)))
                drift = (p-s['shaft_targets'][(model.get_name(), along)]).length()
                report['max_shaft_drift_cm'] = max(report['max_shaft_drift_cm'], drift)
                check(drift < .3, 'shaft stable') if drift >= .3 else None
        s['angles'].append(e.drill_spin_degrees)
        if now-s['at'] < 1.:
            return
        check(e.get_editor_property('bDrillRunning') and len(set(round(a) for a in s['angles'])) > 8, 'Real left mouse input continuously spins both drills')
        check(report['max_shaft_drift_cm'] < .3, 'Both shaft axes remain stable at existing artist offsets')
        key('LeftMouseButton', False)
        s.update(phase='capture', at=now)
    elif s['phase'] == 'capture' and now-s['at'] > .15:
        check(h.capture_preview(s['pc'], str(out / 'dual-drills.png')), 'Captured the new dual-drill materials in UE')
        transition('drills-to-bomb', 'G', s['e'].bomb_pivot, s['components']['BombModel'])

def guard(dt):
    if s.get('busy', True):
        return
    s['busy'] = True
    try:
        tick(dt)
    except Exception:
        finish(False, traceback.format_exc())
    finally:
        if s:
            s['busy'] = False

callback = unreal.register_slate_post_tick_callback(guard)
h.start_floating_preview()
