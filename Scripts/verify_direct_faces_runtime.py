import json
import math
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None, "Start PIE first"
pawn = unreal.GameplayStatics.get_player_character(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
assert generator.direct_room_faces and generator.layout_applied
segments = list(generator.generated_segments)
mining.reset_field()
initial_triangles = [segment.triangle_count for segment in segments]
targets = []
for route in generator.routes:
    targets.extend((route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD), None) for index in range(2))
    targets.append((route.to_room.get_actor_location(), route.to_room.connection_order))
targets = globals().get("targets_override", targets)
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
report = {"passed": False, "scope": "All direct links in the unchanged user layout; actual CharacterMovement and TryMine", "initial_triangles": initial_triangles, "peak_triangles": sum(initial_triangles), "rooms_reached": [], "transitions": []}
state = {"started": time.monotonic(), "progress_time": time.monotonic(), "best": 1e9, "target": 0, "next_cut": 0, "revision": generator.build_revision, "rejected": 0}
cut_timings = []


def trace(first, second):
    return unreal.SystemLibrary.line_trace_single(world, first, second, unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    movement.stop_movement_immediately()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"], "position": str(pawn.get_actor_location()), "target_index": state["target"], "successful_cuts": sum(segment.successful_cuts for segment in segments), "segments": [{"cuts": segment.successful_cuts, "triangles": segment.triangle_count, "state": str(segment.segment_state)} for segment in segments], "current_room": generator.current_room_index, "mining_status": mining.status, "no_runtime_regeneration": generator.build_revision == state["revision"]})
    lamps = pawn.get_components_by_class(unreal.LocalLightComponent)
    report["cut_ms"] = cut_timings
    report["chunk_stats"] = [{"chunks": len(segment.chunks), "largest": segment.largest_chunk_triangles, "removed": segment.simplified_triangles, "completed_background": segment.completed_simplifications, "discarded_background": segment.discarded_simplifications} for segment in segments]
    report["lights"] = [{"name": light.get_name(), "intensity": light.intensity, "visible": light.is_visible(), "radius": light.attenuation_radius} for light in lamps]
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / globals().get("runtime_output", "direct_faces_runtime.json")
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DIRECT_FACES_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        assert editor.get_game_world() is not None, "PIE stopped"
        assert generator.build_revision == state["revision"], "Runtime regeneration erased digging"
        now = time.monotonic()
        report["peak_triangles"] = max(report["peak_triangles"], sum(segment.triangle_count for segment in segments))
        current = generator.current_room_index
        if not report["transitions"] or report["transitions"][-1]["room"] != current:
            report["transitions"].append({"room": current, "active": generator.active_segment_index, "states": [str(segment.segment_state) for segment in segments]})
        if now - state["started"] > 300 or now - state["progress_time"] > 35:
            finish(False, "Character stopped progressing toward destination waypoint")
            return
        position = pawn.get_actor_location()
        target, room_order = targets[state["target"]]
        horizontal = target - position
        horizontal.z = 0
        distance = horizontal.length()
        remaining = distance + abs(position.z - target.z - capsule.get_scaled_capsule_half_height()) * 0.25
        if remaining < state["best"] - 5:
            state["best"] = remaining
            state["progress_time"] = now
        if distance < 80 and abs(position.z - target.z - capsule.get_scaled_capsule_half_height()) < 100:
            if state["target"] in globals().get("space_checkpoints", {}):
                expected = space_checkpoints[state["target"]]
                assert generator.current_room_index == expected, "Entering a cave incorrectly advanced main-room progression"
                report.setdefault("space_checkpoints", []).append({"waypoint": state["target"], "room": generator.current_room_index, "position": str(position)})
            if room_order is not None:
                report["rooms_reached"].append(room_order)
            state["target"] += 1
            state["best"] = 1e9
            state["progress_time"] = now
            if state["target"] == len(targets):
                assert sum(segment.successful_cuts for segment in segments) >= 5, "Route crossed without exercising mining"
                assert generator.current_room_index == len(segments), "Final room progression not committed"
                assert all(segment.segment_state == unreal.MiningSegmentState.FROZEN for segment in segments), "Completed segments did not freeze"
                assert not generator.generated_bedrock.get_editor_property("hidden")
                finish(True, "Reached all rooms through solid direct links, without teleport, flight, collision changes or route regeneration")
            return
        direction = horizontal / max(distance, 0.01)
        pitch = math.degrees(math.atan2(target.z - position.z + capsule.get_scaled_capsule_half_height(), max(distance, 1)))
        aim = unreal.Rotator(pitch=max(-65, min(30, pitch)), yaw=math.degrees(math.atan2(direction.y, direction.x)))
        if now - state["progress_time"] > 0.3:
            obstacle = unreal.SystemLibrary.capsule_trace_single(world, position, position + direction * 110, capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if obstacle and isinstance(obstacle.to_tuple()[9], unreal.DiggableRock):
                hit = obstacle.to_tuple()
                aim = unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), hit[5] - hit[7] * 8)
            else:
                footing = trace(position, position - unreal.Vector(0, 0, 400))
                if footing and isinstance(footing.to_tuple()[9], unreal.DiggableRock):
                    for offset in (0, 90, -90, 180):
                        candidate = unreal.Rotator(pitch=-65, yaw=aim.yaw + offset)
                        view = camera.get_world_location()
                        floor_hit = trace(view, view + unreal.MathLibrary.get_forward_vector(candidate) * mining.reach)
                        if floor_hit and isinstance(floor_hit.to_tuple()[9], unreal.DiggableRock):
                            aim = candidate
                            break
        if state["rejected"]:
            yaw_offsets = (-4, 4, -8, 8)
            aim.yaw += yaw_offsets[(state["rejected"] - 1) % len(yaw_offsets)]
            aim.pitch = max(-68, min(70, aim.pitch + (4 if state["rejected"] % 2 else -4)))
        controller.set_control_rotation(aim)
        pawn.add_movement_input(direction, 1, True)
        if now >= state["next_cut"]:
            if mining.try_mine() and mining.last_mined_rock:
                cut_timings.append(mining.last_mined_rock.last_total_milliseconds)
            state["rejected"] = state["rejected"] + 1 if "rejected" in mining.status.lower() else 0
            if state["rejected"]:
                report["rejected_attempts"] = report.get("rejected_attempts", 0) + 1
            state["next_cut"] = now + 0.34
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("DIRECT_FACES_RUNTIME_ARMED")
