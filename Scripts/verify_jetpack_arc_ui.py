"""Real flight input verifies transient gauge, red-zone overlay and cooldown in the latest map."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
out = Path(unreal.Paths.project_saved_dir()) / 'Validation/JetpackArc'
out.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': [], 'samples': {}}
state = {'phase': 'setup', 'started': time.monotonic(), 'busy': False}

def check(value, label):
    assert value, label
    report['checks'].append(label)

def key(pressed): helper.send_preview_key(state['pc'], 'SpaceBar', pressed)

def sample(name):
    ui = state['ui']; m = state['m']
    report['samples'][name] = {'heat': m.jetpack_heat, 'ui_heat': ui.displayed_heat,
        'gauge_opacity': ui.gauge_opacity, 'warning_opacity': ui.warning_opacity,
        'lock': m.jetpack_overheat_remaining, 'thrust': m.is_flight_thrust_active()}

def shot(name):
    sample(name)
    check(helper.capture_preview(state['pc'], str(out/(name+'.png'))), 'Captured '+name)

def finish(passed, reason=''):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() and 'pc' in state: key(False)
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=passed, reason=reason, elapsed=time.monotonic()-state['started'])
    (out/'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('JETPACK_ARC_RESULT '+json.dumps(report))
    if editor.get_game_world(): levels.editor_request_end_play()

def tick(delta):
    if state['busy']: return
    state['busy'] = True
    try:
        if time.monotonic()-state['started'] > 120: raise RuntimeError('Arc test timeout: '+state['phase'])
        phase = state['phase']
        if phase == 'setup':
            world = editor.get_game_world()
            if not world: return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            if not pc or not pc.mining_gui or not pawn: return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if any(not h.get_editor_property('bReady') for h in halls): return
            gui = pc.mining_gui; ui = gui.jetpack_heat_bar
            m = pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            state.update(world=world, pc=pc, gui=gui, ui=ui, m=m)
            report['character_class'] = pawn.get_class().get_path_name()
            check(m.flight_settings.enabled and m.flight_settings.enable_heat, 'Latest level retains real jetpack gameplay')
            check(ui and gui.jetpack_heat_warning, 'Gauge and original warning artwork are editable HUD children')
            check(abs(ui.heat_bar.get_render_opacity()-.8)<.001, 'Gauge artwork has 80 percent opacity before the automatic fade')
            anchors = ui.slot.get_anchors()
            check(abs(anchors.minimum.x-.5)<.001 and abs(anchors.maximum.x-.5)<.001
                  and abs(anchors.minimum.y-1.)<.001 and abs(anchors.maximum.y-1.)<.001,
                  'Gauge is anchored at the horizontal center of the bottom edge')
            size = ui.slot.get_size(); position = ui.slot.get_position()
            check(abs(size.x-300.)<.001 and abs(size.y-120.)<.001
                  and abs(position.x)<.001 and abs(position.y+40.)<.001,
                  'Dashboard uses a wide 300 by 120 canvas above the bottom edge')
            check(ui.gauge_opacity == 0 and ui.warning_opacity == 0, 'Resting HUD starts with gauge and warning completely hidden')
            check(abs(ui.warning_threshold-.75)<.001, 'Final quarter is the red warning zone')
            check(gui.jetpack_heat_warning.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE, 'Full-screen warning never intercepts input')
            check(all(c.green_segment4 for c in [gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade]), 'Four-level upgrade rings remain bound')
            shot('idle-hidden')
            key(True); state.update(phase='fade-in', start=unreal.GameplayStatics.get_time_seconds(world)); return
        now = unreal.GameplayStatics.get_time_seconds(state['world'])
        ui = state['ui']; m = state['m']
        if phase == 'fade-in' and now-state['start'] >= m.flight_settings.hold_to_ignite_seconds + .06:
            check(0 < ui.gauge_opacity < 1, 'Flight reveals gauge through a short fade instead of a pop')
            state['phase'] = 'yellow'
        elif phase == 'yellow' and now-state['start'] >= 2.5:
            check(m.is_flight_thrust_active() and .45 < ui.displayed_heat < .65, 'Yellow fill follows actual sustained thrust')
            check(ui.gauge_opacity > .99 and ui.warning_opacity == 0, 'Normal heat shows gauge without the screen warning')
            shot('yellow-zone'); state['phase'] = 'danger'
        elif phase == 'danger' and now-state['start'] >= m.flight_settings.hold_to_ignite_seconds + 4.1:
            check(.78 < ui.displayed_heat < .92 and ui.warning_opacity > 0, 'Entering last quarter reveals the red screen warning')
            check(ui.warning_opacity <= ui.warning_max_opacity, 'Warning stays below its editable opacity limit')
            check(abs(state['gui'].jetpack_heat_warning.get_render_opacity()-ui.warning_opacity)<.001, 'Screen overlay tracks the heat warning fade')
            shot('red-zone-warning')
            key(False); state.update(phase='release', release_at=unreal.GameplayStatics.get_time_seconds(state['world']), release_heat=m.jetpack_heat)
        elif phase == 'release' and ui.displayed_heat < .75:
            check(not m.is_flight_thrust_active() and m.jetpack_heat < state['release_heat'], 'Release immediately stops thrust and retracts the fill')
            check(ui.gauge_opacity > .99, 'Gauge stays visible while cooling rather than disappearing at release')
            check(ui.displayed_heat < .75 and ui.warning_opacity > 0, 'Leaving the red zone fades the warning without an instant cut')
            shot('cooling-warning-fade'); state['phase'] = 'empty'
        elif phase == 'empty' and m.jetpack_heat == 0:
            state.update(phase='fade-out', empty_at=now)
        elif phase == 'fade-out' and now-state['empty_at'] >= .43:
            check(0 < ui.gauge_opacity < 1 and ui.warning_opacity == 0, 'Empty gauge briefly remains then smoothly fades out')
            state['before_reverse'] = ui.gauge_opacity
            key(True); state.update(phase='reverse', reverse_at=now)
        elif phase == 'reverse' and now-state['reverse_at'] >= .06:
            check(ui.gauge_opacity > state['before_reverse'], 'Reusing the pack reverses an in-progress fade without hiding')
            state['phase'] = 'overheat'
        elif phase == 'overheat' and m.is_jetpack_overheated():
            check(not m.is_flight_thrust_active() and ui.gauge_opacity > .99, 'Full heat keeps the gauge visible and blocks flight')
            shot('overheat-warning'); key(False)
            state.update(phase='lock-mid', lock_at=now, lock_heat=m.jetpack_heat)
        elif phase == 'lock-mid' and now-state['lock_at'] >= 1:
            check(m.is_jetpack_overheated() and not m.is_flight_thrust_active(), 'Cooling lock still blocks flight')
            check(.55 < ui.displayed_heat < .75 and ui.gauge_opacity > .99, 'Locked heat retracts through the dark groove')
            shot('cooldown-gauge'); state['phase'] = 'recover'
        elif phase == 'recover' and not m.is_jetpack_overheated():
            check(ui.displayed_heat < .03, 'Three-second cooling ends with an empty gauge')
            state.update(phase='rest', recovered_at=now)
        elif phase == 'rest' and now-state['recovered_at'] >= 1:
            check(ui.gauge_opacity == 0 and ui.warning_opacity == 0, 'Recovered idle HUD hides both gauge and full-screen warning')
            shot('rest-hidden')
            helper.send_preview_key(state['pc'], 'Tab', True)
            state.update(phase='tab', tab_at=now)
        elif phase == 'tab' and now-state['tab_at'] >= .4:
            check(state['pc'].show_mouse_cursor and state['pc'].is_look_input_ignored(), 'Existing held-Tab upgrades still expose cursor and lock look')
            helper.send_preview_focused_key(state['pc'], 'Tab', False)
            check(not state['pc'].show_mouse_cursor and not state['pc'].is_look_input_ignored(), 'Releasing Tab still restores mouse look')
            finish(True)
    except Exception:
        if 'ui' in state: sample('failure')
        finish(False, traceback.format_exc())
    finally:
        state['busy'] = False

assert not editor.get_game_world(), 'Do not interrupt an existing preview'
callback = unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
