"""Cold PIE first-hit regression: real player tool, no mining/cache warm-up.

Run with UnrealEditor -ExecutePythonScript=<this file> -RenderOffscreen.
Writes Saved/FirstPickaxeVerification.json; modifies only the PIE world.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
settings = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
settings.set_editor_property('bThrottleCPUWhenNotForeground', False)
state = dict(start=time.perf_counter(), phase='wait', hits=[], frames=[], scans=0)

def finish():
    state['phase'] = 'finished'
    unreal.unregister_slate_post_tick_callback(handle)
    settings.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    data = {k: v for k, v in state.items() if k not in ('pawn', 'pc', 'mining', 'field', 'start', 'last', 'next')}
    frames = sorted(state['frames'])
    data['frame_summary'] = dict(count=len(frames), max_ms=max(frames, default=0),
                                p95_ms=frames[int(.95*len(frames))] if frames else 0)
    (root/'Saved/FirstPickaxeVerification.json').write_text(json.dumps(data, indent=2, default=str))
    unreal.log('FIRST_PICKAXE_RESULT '+json.dumps(data, default=str))
    levels.editor_request_end_play()
    # EndPlay is deferred. Let the PIE world and its render resources finish
    # teardown before shutting down the editor (never request both in one tick).
    state['shutdown_frames'] = 0
    state['shutdown_handle'] = unreal.register_slate_post_tick_callback(quit_after_pie)

def quit_after_pie(dt):
    if editor.get_game_world():
        return
    state['shutdown_frames'] += 1
    if state['shutdown_frames'] < 5:
        return
    unreal.unregister_slate_post_tick_callback(state['shutdown_handle'])
    unreal.SystemLibrary.quit_editor()

def tick(dt):
    if state['phase'] == 'finished':
        return
    now = time.perf_counter()
    try:
        if now-state['start'] > 180:
            raise RuntimeError('First pickaxe regression timed out')
        world = editor.get_game_world()
        if not world:
            return
        if state['phase'] == 'wait':
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not pawn or not halls or not halls[0].ready:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            field = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTreasureField)[0]
            state.update(pawn=pawn, pc=pc, mining=mining, field=field, phase='aim', last=now,
                         ready_world_seconds=unreal.GameplayStatics.get_time_seconds(world))
            # Keep the actual spawn. Aim down toward the natural cave floor.
            pc.set_control_rotation(unreal.Rotator(-65, pc.get_control_rotation().yaw, 0))
            return
        state['frames'].append((now-state['last'])*1000)
        state['last'] = now
        if 'pending_hit' in state:
            state['hits'][state.pop('pending_hit')]['following_frame_ms'] = state['frames'][-1]
        mining = state['mining']
        if state['phase'] == 'aim':
            if not mining.get_editor_property('target_rock'):
                state['scans'] += 1
                if state['scans'] > 32:
                    raise RuntimeError('No diggable rock within reach at actual spawn')
                state['pc'].set_control_rotation(unreal.Rotator(-45 if state['scans'] < 16 else 0, state['scans']*45, 0))
                return
            state['phase'] = 'cut'
            state['next'] = now
        if now < state['next']:
            return
        if len(state['hits']) == 6 and not state.get('vein_aimed'):
            placement = state['field'].get_editor_property('authored_placements')[0]
            surface = placement.surface.translation
            normal = unreal.MathLibrary.transform_location(placement.surface, unreal.Vector(1, 0, 0))-surface
            eye = surface+normal*210
            state['pawn'].get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['pawn'].set_actor_location(eye-unreal.Vector(0, 0, 64), False, True)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, surface))
            state['vein_aimed'] = True
            state['next'] = now+.2
            return
        if len(state['hits']) >= 12:
            assert state['hits'][0]['ok'], 'First player-tool hit did not cut rock'
            assert state['hits'][0]['wall_ms'] < 250, 'First hit still stalls on the game thread'
            assert state['hits'][0]['following_frame_ms'] < 1000, 'First hit still stalls the following render frame'
            assert max(state['frames'][-60:], default=0) < 1000, 'Rendering still stalls after mining'
            assert sum(hit['removed_before_return'] for hit in state['hits'][6:]) > 0, 'Vein cuts did not exercise crystal removal'
            finish()
            return
        counts_before = [item.get_editor_property('crystal_clues').get_instance_count() for item in state['field'].items]
        started = time.perf_counter()
        ok = mining.try_mine()
        elapsed = (time.perf_counter()-started)*1000
        rock = mining.get_editor_property('last_mined_rock')
        counts_after = [item.get_editor_property('crystal_clues').get_instance_count() for item in state['field'].items]
        state['hits'].append(dict(ok=ok, wall_ms=elapsed,
            world_seconds=unreal.GameplayStatics.get_time_seconds(world),
            rock=rock.get_name() if rock else None,
            boolean_ms=rock.last_boolean_milliseconds if rock else None,
            removed_before_return=sum(counts_before)-sum(counts_after)))
        state['pending_hit'] = len(state['hits'])-1
        assert all(a <= b for a, b in zip(counts_after, counts_before)), 'A swing unexpectedly spawned fragments'
        state['next'] = time.perf_counter()+.6
    except Exception:
        state['error'] = traceback.format_exc()
        finish()

handle = unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
