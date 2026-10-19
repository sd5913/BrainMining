"""PIE check of actual subtitle visibility/audio during the inter-line pause."""
import json
import time
import traceback
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/Validation/SubtitleSpacing'
OUT.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
perf = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = perf.get_editor_property('bThrottleCPUWhenNotForeground')
perf.set_editor_property('bThrottleCPUWhenNotForeground', False)
state = dict(phase='start', started=time.monotonic(), busy=False)
report = dict(passed=False, checks=[])

def check(ok, message):
    assert ok, message
    report['checks'].append(message)

def speaking():
    for component in unreal.ObjectIterator(unreal.AudioComponent):
        sound = component.get_editor_property('sound')
        if sound and sound.get_outer() == state['subtitles'] and component.is_playing():
            return True
    return False

def finish(error=None):
    if error:
        report['error'] = error
    else:
        report['passed'] = True
    (OUT / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(state['handle'])
    perf.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    levels.editor_request_end_play()
    unreal.log('SUBTITLE_SPACING_VALIDATION ' + json.dumps(report, ensure_ascii=False))

def tick(dt):
    if state['busy']:
        return
    state['busy'] = True
    try:
        assert time.monotonic() - state['started'] < 100, 'PIE spacing check timed out'
        world = editor.get_game_world()
        if not world:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        for gauge in state.get('gauges', []):
            gauge.set_heat_state(.55, False, True)
            gauge.advance_presentation(1.)
        if state['phase'] == 'start':
            if now < 4:
                return
            for opening in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningOpening):
                opening.start_game()
            state.update(phase='prepare', next=now + 4)
        elif state['phase'] == 'prepare':
            if now < state['next']:
                return
            gi = unreal.GameplayStatics.get_game_instance(world)
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            sub = next(s for s in unreal.ObjectIterator(unreal.SubtitleSubsystem) if s.get_outer() == gi)
            settings = unreal.get_default_object(unreal.load_class(None, '/Script/BrainMining.SubtitleSettings'))
            check(abs(settings.get_editor_property('LineGap') - .35) < .001, 'Shared line gap is 0.35 seconds')
            check(settings.get_editor_property('BottomMargin') == 150, 'Shared bottom margin is raised to 150')
            sub.stop_all()
            sub.play_segment('卡牌玩法1')
            gauge = pc.get_editor_property('MiningGUI').get_editor_property('JetpackHeatBar')
            check(gauge is not None, 'Use the actual HUD jetpack heat gauge for the screenshot')
            gauge.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
            gauges = [gauge]
            state.update(phase='first', subtitles=sub, pc=pc, gauges=gauges)
        elif state['phase'] == 'first':
            widget = next(w for w in unreal.ObjectIterator(unreal.SubtitleWidget) if w.get_owning_player() == state['pc'] and w.is_in_viewport())
            check(widget.is_visible() and speaking(), 'First caption and generated speech play together')
            state['widget'] = widget
            state['phase'] = 'capture'
        elif state['phase'] == 'capture':
            unreal.MiningGUIEditorLibrary.capture_preview(state['pc'], str(OUT / 'raised_subtitle.png'))
            state['phase'] = 'gap'
        elif state['phase'] == 'gap':
            if state['widget'].is_visible():
                return
            check(not speaking(), 'Inter-line pause hides text and stops speech')
            state.update(phase='second', gap_start=now)
        elif state['phase'] == 'second':
            if not state['widget'].is_visible():
                return
            report['observed_gap_seconds'] = now - state['gap_start']
            check(.15 <= report['observed_gap_seconds'] <= .65, 'Visible blank pause separates the two lines')
            check(speaking(), 'Second caption restores generated speech')
            state['subtitles'].stop_all()
            check(unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(state['pc'], 1, False), 'Activate office in PIE fixture')
            office = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PopupCloseStage)[0]
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            pawn.set_actor_location(office.get_actor_transform().transform_location(unreal.Vector(-790, 0, 96)), False, True)
            state['pc'].set_control_rotation(unreal.Rotator(yaw=180))
            state.update(phase='office', next=now + 1.)
        elif state['phase'] == 'office':
            if now < state['next']:
                return
            prompts = [p for p in unreal.ObjectIterator(unreal.PopupClosePromptWidget) if p.get_owning_player() == state['pc']]
            check(not any(p.is_visible() and p.is_in_viewport() for p in prompts), 'Office entrance shows no start-working prompt')
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False

assert not editor.get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['handle'] = unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
