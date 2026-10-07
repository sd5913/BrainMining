import json
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
movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
segments = list(generator.generated_segments)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
old_policy = generator.completed_segment_policy
old_hold = generator.room_entry_hold_time
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
mining.reset_field()
movement.disable_movement()
generator.room_entry_hold_time = 0.15
generator.completed_segment_policy = unreal.CompletedMiningSegments.KEEP_FROZEN
initial_counts = [segment.triangle_count for segment in segments]
report = {"passed": False, "scope": "Targeted lifecycle tests use teleports and disabled movement; traversal is tested separately", "checks": []}
state = {"phase": 0, "time": time.monotonic(), "started": time.monotonic(), "frozen_count": 0}

def check(condition, label):
    assert condition, label
    report["checks"].append(label)

def center(room):
    position = room.get_actor_location()
    position.z += room.room_size.z * room.get_actor_scale3d().z * 0.5
    return position

def face(room, wall):
    direction = [unreal.Vector(1, 0, 0), unreal.Vector(-1, 0, 0), unreal.Vector(0, 1, 0), unreal.Vector(0, -1, 0), unreal.Vector(0, 0, 1), unreal.Vector(0, 0, -1)][wall]
    return center(room) + direction * room.room_size * room.get_actor_scale3d() * 0.5, direction

def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    generator.completed_segment_policy = old_policy
    generator.room_entry_hold_time = old_hold
    mining.reset_field()
    movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "segmented_lifecycle.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SEGMENTED_LIFECYCLE " + json.dumps(report))

def tick(delta):
    try:
        if time.monotonic() - state["time"] < 1.25:
            return
        phase = state["phase"]
        if phase == 0:
            check(generator.active_segment_index == 0, "First segment active after reset")
            check(segments[1].segment_state == unreal.MiningSegmentState.WAITING, "Future segment waits")
            point, direction = face(rooms[0], generator.routes[0].get_editor_property("ResolvedStartWall"))
            check(segments[0].mine_sphere(point + direction * 30, 130), "Active segment can be excavated")
            state["frozen_count"] = segments[0].triangle_count
            check(not segments[1].mine_sphere(center(rooms[1]), 130), "Direct mining call cannot bypass waiting state")
            arrival, outward = face(rooms[1], generator.routes[0].get_editor_property("ResolvedEndWall"))
            half = capsule.get_scaled_capsule_half_height() if abs(outward.z) > 0.5 else capsule.get_scaled_capsule_radius()
            pawn.set_actor_location(arrival - outward * (half - 10), False, True)
        elif phase == 1:
            check(generator.current_room_index == 0, "Overlapping doorway does not advance room")
            pawn.set_actor_location(center(rooms[1]), False, True)
        elif phase == 2:
            check(generator.current_room_index == 1 and generator.active_segment_index == 1, "Full capsule inside room activates next segment")
            check(segments[0].segment_state == unreal.MiningSegmentState.FROZEN, "Old segment freezes")
            check(segments[0].triangle_count == state["frozen_count"] and not segments[0].get_editor_property("hidden"), "Freezing preserves excavated shape and visibility")
            check(not segments[0].mine_sphere(center(rooms[0]), 130), "Frozen segment rejects further edits")
            check(segments[0].triangle_count == state["frozen_count"], "Rejected frozen edit leaves geometry intact")
            point, direction = face(rooms[1], generator.routes[1].get_editor_property("ResolvedStartWall"))
            check(segments[1].mine_sphere(point + direction * 30, 130), "New segment remains independently diggable")
            pawn.set_actor_location(center(rooms[0]), False, True)
        elif phase == 3:
            check(generator.current_room_index == 1 and segments[0].segment_state == unreal.MiningSegmentState.FROZEN, "Moving backward never reactivates old segment")
            pawn.set_actor_location(center(rooms[2]), False, True)
        elif phase == 4:
            check(generator.current_room_index == 2 and generator.active_segment_index == 2, "Second transition succeeds")
            check(segments[0].triangle_count == state["frozen_count"], "Default policy retains older geometry regardless of camera")
            generator.completed_segment_policy = unreal.CompletedMiningSegments.SEAL_AND_RECYCLE
            arrival, outward = face(rooms[1], generator.routes[0].get_editor_property("ResolvedEndWall"))
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(), arrival))
        elif phase == 5:
            check(segments[0].segment_state == unreal.MiningSegmentState.FROZEN, "Visible old entrance is not sealed or recycled")
            controller.set_control_rotation(unreal.Rotator(pitch=0, yaw=0, roll=0))
        elif phase == 6:
            check(segments[0].segment_state == unreal.MiningSegmentState.RECYCLED, "Optional policy recycles safely behind the player")
            check(segments[0].triangle_count == 0 and segments[0].get_editor_property("hidden"), "Recycling clears live mesh instead of hiding only")
            check(segments[0].rock_mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, "Recycled mesh no longer collides")
            check(all(chunk.mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION and chunk.mesh.get_dynamic_mesh().get_triangle_count() == 0 for chunk in segments[0].chunks), "Recycling clears every internal chunk and collision")
            check(segments[1].segment_state == unreal.MiningSegmentState.FROZEN and segments[1].triangle_count > 0, "Immediately previous tunnel remains intact")
            arrival, outward = face(rooms[1], generator.routes[0].get_editor_property("ResolvedEndWall"))
            hit = unreal.SystemLibrary.line_trace_single(world, arrival - outward * 60, arrival + outward * 60, unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            check(hit is not None and isinstance(hit.to_tuple()[9], unreal.DynamicMeshActor), "Permanent non-diggable seal blocks the recycled entrance")
            report["seal_name"] = hit.to_tuple()[9].get_name()
            mining.reset_field()
        elif phase == 7:
            check(generator.current_room_index == 0 and generator.active_segment_index == 0, "Reset restores progression")
            check(initial_counts == [segment.triangle_count for segment in segments], "Reset restores even recycled meshes")
            check(all(segment.successful_cuts == 0 for segment in segments), "Reset clears all segment counters")
            check(all(all(chunk.mesh.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for chunk in segment.chunks) if segment.chunked else segment.rock_mesh.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for segment in segments), "Reset restores collision")
            seals = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DynamicMeshActor) if actor.get_name() == report["seal_name"]]
            check(not seals, "Reset removes runtime seal")
            pawn.set_actor_location(center(rooms[2]), False, True)
        else:
            check(generator.current_room_index == 0, "Cannot skip the expected room by entering a later room")
            finish(True, "Boundary, waiting, freeze, no backtracking, safe recycle and reset checks passed")
            return
        state["phase"] += 1
        state["time"] = time.monotonic()
    except Exception:
        finish(False, traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("SEGMENTED_LIFECYCLE_ARMED")
