"""Real tool input and jetpack heat verify the supplied centered reticles and overlay."""
import json, time, traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
h = unreal.MiningGUIEditorLibrary
out = Path(unreal.Paths.project_saved_dir()) / 'Validation/ToolReticles'
out.mkdir(parents=True, exist_ok=True)
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
report = {'passed': False, 'checks': [], 'samples': {}}
s = {'phase': 'setup', 'started': time.monotonic(), 'busy': False}

def check(value, label):
    assert value, label
    report['checks'].append(label)

def key(name, pressed):
    h.send_preview_key(s['pc'], name, pressed)

def press(name):
    key(name, True)
    key(name, False)

def aim(tool):
    check([r.is_visible() for r in s['reticles']] == [i == tool for i in range(3)],
          'Only ' + ['pickaxe', 'drill', 'bomb'][tool] + ' reticle is visible')

def shot(name):
    check(h.capture_preview(s['pc'], str(out / (name + '.png'))), 'Captured ' + name)

def sample(name):
    report['samples'][name] = {'heat': s['m'].jetpack_heat, 'opacity': s['ui'].warning_opacity,
                              'lock': s['m'].jetpack_overheat_remaining}

def finish(ok, reason=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() and s.get('pc'):
        for k in ['G', 'V', 'SpaceBar', 'Tab', 'LeftMouseButton']:
            key(k, False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old)
    report.update(passed=ok, reason=reason, elapsed=time.monotonic()-s['started'])
    (out / 'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('TOOL_RETICLES_VERIFIED ' + json.dumps(report))
    if editor.get_game_world():
        levels.editor_request_end_play()
    s.clear()

def tick(dt):
    assert time.monotonic()-s['started'] < 120, 'Reticle verification timeout'
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
        gui = pc.mining_gui
        report['hud_path'] = gui.get_path_name()
        report['reticle_paths'] = [image.get_path_name() for image in unreal.ObjectIterator(unreal.Image) if image.get_name().startswith('Reticle')]
        children = {image.get_name(): image for image in unreal.ObjectIterator(unreal.Image)
                    if image.get_path_name().startswith(gui.get_path_name())}
        reticles = [children.get('Reticle' + name) for name in ['Pickaxe', 'Drill', 'Bomb']]
        check(all(reticles), 'All three editable reticles exist in the saved HUD')
        s.update(world=world, pc=pc, pawn=pawn, gui=gui, reticles=reticles, ui=gui.jetpack_heat_bar,
                 m=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent))
        for r in reticles:
            a, p, size = r.slot.get_anchors(), r.slot.get_position(), r.slot.get_size()
            check(a.minimum == unreal.Vector2D(.5, .5) and a.maximum == a.minimum and p == unreal.Vector2D(), r.get_name() + ' stays at screen center')
            check(size == unreal.Vector2D(64, 64) and r.slot.get_alignment() == unreal.Vector2D(.5, .5), r.get_name() + ' has a small centered canvas')
            check(r.slot.get_z_order() > gui.jetpack_heat_warning.slot.get_z_order(), r.get_name() + ' renders above the heat warning')
            check(r.get_dynamic_material() is not None, r.get_name() + ' masks the source black background')
        check(abs(s['ui'].warning_max_opacity-.68) < .001, 'Saved overheat warning intensity increased from .55 to .68')
        check(gui.jetpack_heat_warning.slot.get_z_order() == -100, 'Overheat artwork stays underneath all HUD controls')
        check(gui.jetpack_heat_warning.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'Screen effect cannot intercept clicks or keys')
        check(all(c.green_segment4 for c in [gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade]), 'Four upgrade levels are still present')
        aim(0)
        shot('pickaxe')
        press('V')
        s.update(phase='drill', at=unreal.GameplayStatics.get_time_seconds(world))
        return
    now = unreal.GameplayStatics.get_time_seconds(s['world'])
    if s['phase'] == 'drill' and now-s['at'] > .35:
        aim(1)
        shot('drill')
        press('G')
        s.update(phase='bomb', at=now)
    elif s['phase'] == 'bomb' and now-s['at'] > .35:
        aim(2)
        shot('bomb')
        press('G')
        s.update(phase='pickaxe', at=now)
    elif s['phase'] == 'pickaxe' and now-s['at'] > .35:
        aim(0)
        key('Tab', True)
        s.update(phase='menu', at=now)
    elif s['phase'] == 'menu' and now-s['at'] > .35:
        check(not any(r.is_visible() for r in s['reticles']), 'Upgrade panel hides aiming art without showing the legacy crosshair')
        h.send_preview_focused_key(s['pc'], 'Tab', False)
        key('Tab', False)
        s.update(phase='flight-start', at=now)
    elif s['phase'] == 'flight-start' and now-s['at'] > .2:
        aim(0)
        key('SpaceBar', True)
        s.update(phase='heat', at=now, warning_samples=[])
    elif s['phase'] == 'heat':
        ui, m = s['ui'], s['m']
        if m.jetpack_heat > .75:
            s['warning_samples'].append(ui.warning_opacity)
        if not m.is_jetpack_overheated():
            return
        check(not m.is_flight_thrust_active(), 'Actual Space flight is blocked when the original heat system overheats')
        check(ui.warning_opacity > .55 and ui.warning_opacity <= .681, 'Overheat reaches the stronger red screen effect')
        check(len(s['warning_samples']) > 2 and min(s['warning_samples']) < .4, 'Red effect still fades in through the warning zone')
        check(abs(s['gui'].jetpack_heat_warning.get_render_opacity()-ui.warning_opacity) < .001, 'HUD layer follows the warning fade')
        aim(0)
        sample('overheat')
        shot('overheat-with-clear-ui')
        key('SpaceBar', False)
        s.update(phase='cool', at=unreal.GameplayStatics.get_time_seconds(s['world']), full_heat=m.jetpack_heat, opacities=[])
    elif s['phase'] == 'cool':
        ui, m = s['ui'], s['m']
        s['opacities'].append(ui.warning_opacity)
        if now-s['at'] > 1. and 'cool_checked' not in s:
            check(m.is_jetpack_overheated() and not m.is_flight_thrust_active() and m.jetpack_heat < s['full_heat'], 'Cooling still retracts heat while flight is locked')
            sample('cooldown')
            s['cool_checked'] = True
        if m.is_jetpack_overheated():
            return
        s.update(phase='rest', at=now)
    elif s['phase'] == 'rest' and now-s['at'] > 1.:
        check(s['ui'].warning_opacity == 0 and s['ui'].gauge_opacity == 0, 'Both heat gauge and warning disappear after cooling')
        check(any(0 < opacity < .3 for opacity in s['opacities']), 'Warning fades out instead of cutting abruptly')
        aim(0)
        finish(True)

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
