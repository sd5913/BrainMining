import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowBranches_Demo")
levels.editor_set_viewport_realtime(True)
world = editor.get_editor_world()
layout = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = layout.hollow_region
chambers = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowChamber), key=lambda chamber: chamber.connection_position)
targets = []
checkpoints = {}
caves = []
last_fraction = 0
length = region.path.get_spline_length()
for chamber in chambers:
    fraction = chamber.connection_position
    for index in range(region.path.get_number_of_spline_points()):
        distance = region.path.get_distance_along_spline_at_spline_point(index)
        if last_fraction < distance / length < fraction:
            targets.append((region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, region.passage_radius), None))
    start = chamber.path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, chamber.branch_radius)
    targets.append((start, None))
    targets.append((chamber.get_actor_location(), None))
    checkpoints[len(targets) - 1] = 0
    targets.append((start, None))
    caves.append({"name": chamber.get_actor_label(), "floor": chamber.get_actor_location(), "half": chamber.chamber_size * 0.5})
    last_fraction = fraction
for index in range(region.path.get_number_of_spline_points()):
    if region.path.get_distance_along_spline_at_spline_point(index) / length > last_fraction:
        targets.append((region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, region.passage_radius), None))
targets.append((region.to_room.get_actor_location(), 1))
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
report = {"passed": False, "visited": [], "map_labels": [], "map_views": []}
state = {"started": time.monotonic(), "phase": 0, "runtime_started": 0}


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() is not None and (passed or not globals().get("keep_failed_play", False)):
        levels.editor_request_end_play()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (output / "hollow_branches_runtime.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_BRANCHES_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        if time.monotonic() - state["started"] > 370:
            finish(False, "Traversal timed out")
        elif state["phase"] == 0 and time.monotonic() - state["started"] > 3:
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            state["runtime_started"] = time.time()
            runpy.run_path(str(Path(unreal.Paths.project_dir()) / "Scripts" / "verify_hollow_branches_walk.py"), init_globals={"targets_override": targets, "space_checkpoints": checkpoints})
            state["phase"] = 2
        elif state["phase"] == 2:
            play = editor.get_game_world()
            pawn = unreal.GameplayStatics.get_player_character(play, 0)
            generator = unreal.GameplayStatics.get_all_actors_of_class(play, unreal.MiningTunnelGenerator)[0]
            controller = unreal.GameplayStatics.get_player_controller(play, 0)
            minimap = controller.get_hud().get_component_by_class(unreal.MiningDeveloperMapComponent)
            position = pawn.get_actor_location()
            for cave in caves:
                offset = position - cave["floor"]
                if abs(offset.x) < cave["half"].x - 50 and abs(offset.y) < cave["half"].y - 50 and 0 < offset.z < cave["half"].z * 2:
                    assert generator.current_room_index == 0, "Cave advanced progression"
                    if cave["name"] not in report["visited"]:
                        report["visited"].append(cave["name"])
                    if cave["name"] in minimap.location_label and minimap.location_label not in report["map_labels"]:
                        report["map_labels"].append(minimap.location_label)
                        for unused in range(3):
                            report["map_views"].append(str(minimap.view))
                            minimap.cycle_view()
            result_file = output / "hollow_branches_traversal.json"
            if result_file.exists() and result_file.stat().st_mtime >= state["runtime_started"]:
                result = json.loads(result_file.read_text(encoding="utf-8"))
                assert result["passed"], result.get("reason")
                assert len(report["visited"]) == 2 and len(report["map_labels"]) == 2, "Both caves and live map labels must be exercised"
                report["successful_cuts"] = result["successful_cuts"]
                report["chunk_stats"] = result["chunk_stats"]
                pawn.get_component_by_class(unreal.MiningComponent).reset_field()
                assert generator.current_room_index == 0 and all(segment.successful_cuts == 0 for segment in generator.generated_segments)
                report["reset_passed"] = True
                finish(True, "Walked into open cave, mined into hidden cave, returned to main, reached B; same-region ownership and developer map verified")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
