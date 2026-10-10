import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo")
world = editor.get_editor_world()
layout = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
layout.set_editor_property("auto_rebuild", False)
regions = sorted(layout.hollow_regions, key=lambda region: region.from_room.connection_order)
caves = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowChamber)
room_floors = [regions[0].from_room.get_actor_location()] + [region.to_room.get_actor_location() for region in regions]
targets = []
checkpoints = {}
cave_floors = []
for region_index, region in enumerate(regions):
    chamber = next(chamber for chamber in caves if chamber.region == region)
    fraction = chamber.connection_position
    length = region.path.get_spline_length()
    for index in range(region.path.get_number_of_spline_points()):
        if region.path.get_distance_along_spline_at_spline_point(index) / length < fraction:
            targets.append((region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, region.passage_radius), None))
    start = chamber.path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, chamber.branch_radius)
    targets.extend([(start, None), (chamber.get_actor_location(), None)])
    checkpoints[len(targets) - 1] = region_index
    cave_floors.append(chamber.get_actor_location())
    targets.append((start, None))
    for index in range(region.path.get_number_of_spline_points()):
        if region.path.get_distance_along_spline_at_spline_point(index) / length > fraction:
            targets.append((region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, region.passage_radius), None))
    targets.append((room_floors[region_index + 1], region_index + 1))
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
report = {"passed": False, "checks": [], "map_labels": []}
state = {"started": time.monotonic(), "phase": 0, "step": 0, "time": time.monotonic()}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (output / "hollow_chain_runtime.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_CHAIN_RUNTIME " + json.dumps(report))
    if "HollowChainAutoQuit" in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        assert time.monotonic() - state["started"] < 680, "Suite timed out"
        if state["phase"] == 0:
            if time.monotonic() - state["started"] < 3:
                return
            levels.editor_request_begin_play()
            state["phase"] = 1
            return
        play = editor.get_game_world()
        if play is None:
            return
        pawn = unreal.GameplayStatics.get_player_character(play, 0)
        if pawn is None:
            return
        generator = unreal.GameplayStatics.get_all_actors_of_class(play, unreal.MiningTunnelGenerator)[0]
        mining = pawn.get_component_by_class(unreal.MiningComponent)
        movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
        controller = unreal.GameplayStatics.get_player_controller(play, 0)
        segments = list(generator.generated_segments)
        if state["phase"] == 1:
            generator.completed_segment_policy = unreal.CompletedMiningSegments.KEEP_FROZEN
            state["run_started"] = time.time()
            state["initial"] = [segment.triangle_count for segment in segments]
            runpy.run_path(str(Path(unreal.Paths.project_dir()) / "Scripts" / "verify_hollow_branches_walk.py"), init_globals={"targets_override": targets, "space_checkpoints": checkpoints, "final_room": 3, "timeout_seconds": 600, "output_name": "hollow_chain_traversal.json"})
            state["phase"] = 2
            return
        if state["phase"] == 2:
            minimap = controller.get_hud().get_component_by_class(unreal.MiningDeveloperMapComponent)
            if "CAVE_" in minimap.location_label and minimap.location_label not in report["map_labels"]:
                report["map_labels"].append(minimap.location_label)
            result_file = output / "hollow_chain_traversal.json"
            if not result_file.exists() or result_file.stat().st_mtime < state["run_started"]:
                return
            result = json.loads(result_file.read_text(encoding="utf-8"))
            check(result["passed"], result.get("reason"))
            check(len(result["space_checkpoints"]) == 3, "Real movement enters a cave in every region without advancing progression")
            check(len(report["map_labels"]) >= 3, "Developer map locates all three caves")
            check(all(segment.successful_cuts > 0 for segment in segments), "Every region supports independent real mining")
            report["cuts_per_region"] = [segment.successful_cuts for segment in segments]
            report["chunk_stats"] = result["chunk_stats"]
            report["no_runtime_regeneration"] = result["no_runtime_regeneration"]
            mining.reset_field()
            movement.disable_movement()
            generator.room_entry_hold_time = 0.15
            generator.recycle_hidden_time = 0.5
            state["phase"] = 3
            state["time"] = time.monotonic()
            return
        if time.monotonic() - state["time"] < 1.4:
            return
        step = state["step"]
        def place(floor):
            pawn.set_actor_location(floor + unreal.Vector(0, 0, 150), False, True)
        if step == 0:
            check(generator.current_room_index == 0 and [segment.triangle_count for segment in segments] == state["initial"], "Reset restores the entire chain after traversal")
            place(room_floors[2])
        elif step == 1:
            check(generator.current_room_index == 0, "Cannot skip the expected room")
            place(room_floors[1] - unreal.Vector(600, 0, 0))
        elif step == 2:
            check(generator.current_room_index == 0, "Partial capsule entry does not advance")
            place(room_floors[1])
        elif step == 3:
            check(generator.current_room_index == 1 and generator.active_segment_index == 1, "Entering second room activates only second region")
            check(segments[0].segment_state == unreal.MiningSegmentState.FROZEN, "Previous region freezes")
            guide = segments[0].guide_material_volumes[0]
            check(not segments[0].mine_sphere(guide.start, 130), "Frozen region cannot be mined")
            place(cave_floors[1])
        elif step == 4:
            check(generator.current_room_index == 1, "A side cave is not a main progression room")
            place(room_floors[2])
        elif step == 5:
            check(generator.current_room_index == 2 and generator.active_segment_index == 2, "Third region activates")
            generator.completed_segment_policy = unreal.CompletedMiningSegments.SEAL_AND_RECYCLE
            controller.set_control_rotation(unreal.Rotator(pitch=0, yaw=180, roll=0))
        elif step == 6:
            check(segments[0].segment_state == unreal.MiningSegmentState.FROZEN, "Visible old entrance is not recycled")
            controller.set_control_rotation(unreal.Rotator(pitch=0, yaw=0, roll=0))
        elif step == 7:
            check(segments[0].segment_state == unreal.MiningSegmentState.RECYCLED, "Old region recycles only behind the player")
            check(segments[0].triangle_count == 0 and all(chunk.mesh.get_dynamic_mesh().get_triangle_count() == 0 and chunk.mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for chunk in segments[0].chunks), "Recycling actually releases active geometry and collision")
            check(segments[1].segment_state == unreal.MiningSegmentState.FROZEN and segments[1].triangle_count > 0, "Immediately previous region remains available")
            face = room_floors[1] + unreal.Vector(-600, 0, 200)
            hit = unreal.SystemLibrary.line_trace_single(play, face - unreal.Vector(60, 0, 0), face + unreal.Vector(60, 0, 0), unreal.TraceTypeQuery.ECC_VISIBILITY, True, [pawn], unreal.DrawDebugTrace.NONE, True)
            check(hit is not None and isinstance(hit.to_tuple()[9], unreal.DynamicMeshActor), "Permanent bedrock seal blocks the recycled region")
            place(room_floors[3])
        elif step == 8:
            check(generator.current_room_index == 3 and generator.active_segment_index == -1, "Final room leaves no active digging region")
            mining.reset_field()
        else:
            check(generator.current_room_index == 0 and [segment.triangle_count for segment in segments] == state["initial"], "Reset restores even recycled regions")
            check(all(segment.successful_cuts == 0 and segment.max_triangles == 30000 and len(segment.chunks) <= 128 for segment in segments), "Reset retains original budgets and clears cut history")
            check(all(chunk.mesh.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for segment in segments for chunk in segment.chunks), "Reset restores chunk collision")
            finish(True, "Three-region traversal, caves, independent mining, freeze, safe recycling and reset passed")
            return
        state["step"] += 1
        state["time"] = time.monotonic()
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("HOLLOW_CHAIN_RUNTIME_ARMED")
