"""Record and verify the real MainGame fountain in an isolated, unsaved PIE world.

Run with -NoSteam and a dedicated -UserDir. No settings or map assets are saved.
Frames carry real timestamps for video encoding without speeding up the footage.
"""
import json
import math
import time
import traceback
from datetime import datetime
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/Review/HeavenDuckGush' / datetime.now().strftime('take-%Y%m%d-%H%M%S')
FRAMES = OUT / 'frames'
FRAMES.mkdir(parents=True, exist_ok=True)
(OUT.parent / 'latest-take.txt').write_text(str(OUT), encoding='utf8')
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper = unreal.MiningGUIEditorLibrary
s = dict(phase=0, next=0, started=time.monotonic(), busy=False, frames=[], samples=[], checks=[])


def actors(cls):
    return unreal.GameplayStatics.get_all_actors_of_class(s['world'], cls)


def check(ok, label):
    assert ok, label
    s['checks'].append(label)


def finish(error=None):
    if s.get('done'):
        return
    s['done'] = True
    report = dict(passed=error is None, error=error, checks=s['checks'], frames=s['frames'],
                  samples=s['samples'], source_map='/Game/MainGame/Maps/Lvl_MainGame',
                  camera='temporary review camera; actual player pawn triggers the production fountain')
    if s.get('audio'):
        try:
            unreal.AudioMixerLibrary.stop_recording_output(s['world'], unreal.AudioRecordingExportType.WAV_FILE,
                                                         'live-audio', str(OUT))
        except Exception as exc:
            report['audio_error'] = str(exc)
    (OUT / 'recording.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    cleanup = dict(at=time.monotonic()+5)

    def close(dt):
        if ed.get_game_world() or time.monotonic() < cleanup['at']:
            return
        if not cleanup.get('unloaded'):
            # Drop Python references before unloading the editor's large campaign world.
            s.clear()
            s.update(done=True, busy=False)
            levels.load_level('/Engine/Maps/Entry')
            cleanup.update(unloaded=True, at=time.monotonic()+3)
            return
        unreal.unregister_slate_post_tick_callback(cleanup['handle'])
        unreal.SystemLibrary.quit_editor()
    cleanup['handle'] = unreal.register_slate_post_tick_callback(close)
    unreal.log('HEAVEN_DUCK_REVIEW_FINISHED ' + str(OUT) + ' passed=' + str(error is None))


def place(distance, lateral=0):
    t = s['toilet']
    p = t.get_actor_location() + s['toward']*distance + s['side']*lateral + unreal.Vector(0, 0, 55)
    s['pawn'].set_actor_location(p, False, True)
    # A slightly offset, low review camera makes the entire flight arc and floor visible.
    eye = t.get_actor_location() + s['toward']*(distance+400) + s['side']*400 + unreal.Vector(0, 0, 80)
    target = t.water_outlet.get_world_location() + s['toward']*180 + unreal.Vector(0, 0, 80)
    s['camera'].set_actor_location(eye, False, True)
    s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)


def tick(dt):
    if s['busy'] or s.get('done'):
        return
    s['busy'] = True
    try:
        now = time.monotonic()
        assert now-s['started'] < 420, 'Review timed out'
        if now < s['next']:
            return
        if s['phase'] == 0:
            assert '-nosteam' in unreal.SystemLibrary.get_command_line().lower()
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            camera = api.spawn_actor_from_class(unreal.CameraActor, unreal.Vector())
            camera.tags = ['HeavenDuckReviewCamera']
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(), 'mining.SaveEnabled 0')
            helper.start_floating_preview()
            s.update(phase=1, next=now+5)
            return
        world = ed.get_game_world()
        if not world:
            return
        s['world'] = world
        if s['phase'] == 1:
            hall = actors(unreal.MiningExhibitHall)[0]
            if not hall.ready:
                return
            for opening in actors(unreal.MiningOpening):
                if not opening.finished:
                    opening.start_game()
                    s['next'] = now+1
                    return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            index = next(i for i, area in enumerate(hall.areas) if str(area.tag) == 'BM_Campaign_MainGame03')
            check(helper.prepare_collection_preview_area(pc, index, False), 'Actual campaign heaven area activated')
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pawn.set_actor_hidden_in_game(True)
            camera = unreal.GameplayStatics.get_all_actors_with_tag(world, 'HeavenDuckReviewCamera')[0]
            camera.camera_component.set_field_of_view(72)
            pc.set_view_target_with_blend(camera, 0)
            toilet = next(t for t in actors(unreal.HeavenToilet) if t.get_actor_location().z > 0)
            frame = actors(unreal.HeavenToiletWater)[0].spill_coordinate_frame
            toward = unreal.MathLibrary.transform_direction(frame, unreal.Vector(-1, 0, 0))
            side = unreal.MathLibrary.transform_direction(frame, unreal.Vector(0, 1, 0))
            s.update(pc=pc, pawn=pawn, camera=camera, toilet=toilet, toward=toward, side=side)
            check(abs(toilet.duck_burst_interval-3.2) < .01, 'Original ambient interval retained')
            check(abs(toilet.gush_seconds-10) < .001, 'Authored campaign fountain uses 10-second gushes')
            helper.resize_preview_viewport(pc, 1280, 720)
            unreal.SystemLibrary.execute_console_command(world, 'DisableAllScreenMessages')
            place(1100)
            s.update(phase=2, next=now+4)
            return
        if s['phase'] == 2:
            s.update(phase=3, record_started=now, world_start=unreal.GameplayStatics.get_time_seconds(world))
            try:
                unreal.AudioMixerLibrary.start_recording_output(world, 65)
                s['audio'] = True
            except Exception:
                pass
        t = s['toilet']
        age = unreal.GameplayStatics.get_time_seconds(world)-s['world_start']
        if age < 7:
            stage = 'ambient'
            place(1100)
        elif age < 10:
            stage = 'approach'
            place(1100-(age-7)/3*540)
        elif age < 39:
            stage = 'gush_cycles' if t.gushing else 'gush_pause'
            # Fixed review camera: move the real pawn left/right, then stop to check convergence.
            lateral = 300*math.sin(max(0, min(age, 35)-11)*.6) if age > 11 else 0
            if age > 35:
                lateral *= max(0, 1-(age-35)/2)
                stage = 'gush_settle' if t.gushing else 'gush_pause'
            place(560, lateral)
        elif age < 42:
            stage = 'leave'
            place(560+(age-39)/3*540)
        else:
            stage = 'ambient_return'
            place(1100)
        offset = s['pawn'].get_actor_location()-t.water_outlet.get_world_location()
        target_yaw = math.degrees(math.atan2(offset.y, offset.x))
        sample = dict(seconds=age, stage=stage, gushing=t.gushing, nearby=t.gush_nearby,
                      launched=t.ducks_launched, recycled=t.ducks_recycled, active=t.active_ducks,
                      flying=t.flying_ducks, direction=list(t.gush_direction.to_tuple()), target_yaw=target_yaw, dt=dt)
        s['samples'].append(sample)
        if now >= s.get('next_frame', 0):
            path = FRAMES / ('frame-%06d.jpg' % len(s['frames']))
            assert helper.capture_preview_video_frame(s['pc'], str(path), 91)
            s['frames'].append(dict(file=path.name, seconds=now-s['record_started'], world_seconds=age, stage=stage))
            s['next_frame'] = now+1/30
        if age >= 47:
            samples = s['samples']
            ambient = [v for v in samples if 1 < v['seconds'] < 6.8]
            check(all(not v['nearby'] for v in ambient), 'Distant player keeps ambient mode')
            check(5 <= ambient[-1]['launched']-ambient[0]['launched'] <= 14, 'Ambient emits original 5-7 duck bursts')
            near = [v for v in samples if 10 < v['seconds'] < 38.8]
            check(max(v['flying'] for v in near) >= 35, 'Dense arc contains at least 35 airborne ducks')
            check(max(v['active'] for v in samples) <= t.max_ducks, 'Visible ducks stay within the fixed pool')
            check(near[-1]['recycled'] > 30, 'Ground ducks recycled while emission continues')
            check(near[-1]['launched']-near[0]['launched'] > 300, 'Multiple continuous gushes keep emitting under load')
            starts = [b for a, b in zip(near, near[1:]) if not a['gushing'] and b['gushing']]
            check(len(starts) >= 2, 'Pause-to-gush cycle repeats')
            def yaw(v):
                return math.degrees(math.atan2(v['direction'][1], v['direction'][0]))
            def angle(a, b):
                return abs((a-b+180)%360-180)
            first_start = next(v for v in samples if v['gushing'])
            cycle_start = first_start['seconds']
            pause_start = None
            complete_gushes = 0
            for a, b in zip(samples, samples[1:]):
                if a['gushing'] and b['gushing']:
                    assert angle(yaw(a), yaw(b)) <= t.gush_turn_speed*(b['seconds']-a['seconds'])+.05, 'Aim turned faster than its slow-follow limit'
                if a['gushing'] and not b['gushing'] and b['nearby']:
                    assert abs(b['seconds']-cycle_start-10) < .18, 'Gush duration is not 10 seconds'
                    complete_gushes += 1
                    pause_start = b['seconds']
                if not a['gushing'] and b['gushing']:
                    cycle_start = b['seconds']
                    if pause_start is not None:
                        assert abs(cycle_start-pause_start-2.5) < .18, 'Pause duration changed'
                if a['nearby'] and b['nearby'] and not a['gushing'] and not b['gushing']:
                    assert a['launched'] == b['launched'], 'Ducks emitted during pause'
            check(complete_gushes >= 2, 'Two complete 10-second gushes and 2.5-second pauses verified')
            moving = [v for v in near if 12 < v['seconds'] < 19 and v['gushing']]
            check(max(angle(yaw(a), yaw(b)) for a in moving for b in moving) > 25, 'Emission direction follows lateral player movement')
            check(max(angle(yaw(v), v['target_yaw']) for v in moving) > 5, 'Aim follows with visible lag rather than snapping')
            settled = [v for v in near if v['seconds'] > 38.5]
            check(all(angle(yaw(v), v['target_yaw']) < 4 for v in settled), 'Aim catches up after the player stops')
            check(True, 'Slow turn-rate bound and no duck emission during pauses verified')
            check(all(not v['nearby'] for v in samples if v['seconds'] > 43), 'Leaving restores ambient mode')
            check(t.water_running and t.water_jet.is_active(), 'Original continuous water remains active')
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        s['busy'] = False


unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle = unreal.register_slate_post_tick_callback(tick)
