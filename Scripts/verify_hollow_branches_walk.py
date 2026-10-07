import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None
pawn = unreal.GameplayStatics.get_player_character(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
mining = pawn.get_component_by_class(unreal.MiningComponent)
movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
segments = list(generator.generated_segments)
mining.reset_field()
targets = targets_override
performance = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = performance.get_editor_property("bThrottleCPUWhenNotForeground")
performance.set_editor_property("bThrottleCPUWhenNotForeground", False)
state = {"started": time.monotonic(), "progress": time.monotonic(), "best": 1e9, "target": 0, "next_cut": 0, "next_jump": 0, "revision": generator.build_revision, "previous": pawn.get_actor_location() - unreal.Vector(0, 0, capsule.get_scaled_capsule_half_height())}
report = {"passed": False, "space_checkpoints": [], "cut_ms": [], "scope": "Real CharacterMovement and TryMine; aim above route floor; no teleport/flight/collision changes/regeneration during traversal"}


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
    performance.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"], "target_index": state["target"], "position": str(pawn.get_actor_location()), "mining_status": mining.status, "successful_cuts": sum(segment.successful_cuts for segment in segments), "room": generator.current_room_index, "no_runtime_regeneration": generator.build_revision == state["revision"]})
    report["chunk_stats"] = [{"chunks": len(segment.chunks), "largest": segment.largest_chunk_triangles} for segment in segments]
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_branches_traversal.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_BRANCHES_WALK " + json.dumps(report))


def tick(delta):
    try:
        assert editor.get_game_world() is not None
        assert generator.build_revision == state["revision"]
        now = time.monotonic()
        if now - state["started"] > 300 or now - state["progress"] > 30:
            finish(False, "Character stopped progressing")
            return
        position = pawn.get_actor_location()
        target, room_order = targets[state["target"]]
        horizontal = target - position
        horizontal.z = 0
        distance = horizontal.length()
        height_error = abs(position.z - capsule.get_scaled_capsule_half_height() - target.z)
        remaining = distance + height_error * 0.25
        if remaining < state["best"] - 5:
            state["best"] = remaining
            state["progress"] = now
        if distance < 70 and height_error < 85:
            if state["target"] in space_checkpoints:
                assert generator.current_room_index == space_checkpoints[state["target"]]
                report["space_checkpoints"].append(state["target"])
            state["previous"] = target
            state["target"] += 1
            state["best"] = 1e9
            state["progress"] = now
            if state["target"] == len(targets):
                assert generator.current_room_index == 1
                assert all(segment.segment_state == unreal.MiningSegmentState.FROZEN for segment in segments)
                assert sum(segment.successful_cuts for segment in segments) >= 3
                finish(True, "Walked both caves and reached B through normal movement and mining")
            return
        direction = horizontal / max(distance, 0.01)
        ahead = position + direction * min(distance, mining.reach * 0.6)
        leg = target - state["previous"]
        length_squared = leg.x * leg.x + leg.y * leg.y
        fraction = max(0, min(1, ((ahead.x - state["previous"].x) * leg.x + (ahead.y - state["previous"].y) * leg.y) / max(1, length_squared)))
        floor = state["previous"].z + leg.z * fraction
        ahead.z = floor + mining.radius + mining.downward_offset + 3
        if now - state["progress"] > 0.18:
            obstruction = unreal.SystemLibrary.capsule_trace_single(world, position, position + direction * 100, capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if obstruction and isinstance(obstruction.to_tuple()[9], unreal.DiggableRock):
                values = obstruction.to_tuple()
                ahead = values[5] - values[7] * 8
                if values[5].z < floor - 30:
                    ahead.z = floor + mining.radius + mining.downward_offset
            report["last_obstruction"] = str(obstruction.to_tuple()) if obstruction else None
            report["last_aim"] = str(ahead)
            report["last_floor"] = floor
        aim = unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), ahead)
        controller.set_control_rotation(aim)
        pawn.add_movement_input(direction, 1, True)
        pawn.stop_jumping()
        if now - state["progress"] > 0.4 and now >= state["next_jump"] and movement.is_moving_on_ground():
            pawn.jump()
            state["next_jump"] = now + 1
            report["jumps"] = report.get("jumps", 0) + 1
        if now >= state["next_cut"] and now - state["progress"] > 0.18:
            if mining.try_mine() and mining.last_mined_rock:
                report["cut_ms"].append(mining.last_mined_rock.last_total_milliseconds)
            state["next_cut"] = now + 0.34
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
