import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
scripts = Path(unreal.Paths.project_dir()) / "Scripts"
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo")
levels.editor_set_viewport_realtime(True)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = generator.hollow_region
targets = [(region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) - unreal.Vector(0, 0, region.passage_radius), None) for index in range(region.path.get_number_of_spline_points())]
targets.append((region.to_room.get_actor_location(), 1))
report = {"passed": False, "reload_chunks": len(generator.generated_fill.chunks), "reload_guides": len(generator.generated_fill.guide_material_volumes)}
state = {"started": time.monotonic(), "phase": 0, "runtime_started": 0}


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (output / "hollow_region_runtime.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_REGION_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        if time.monotonic() - state["started"] > 370:
            finish(False, "Traversal timed out")
        elif state["phase"] == 0 and time.monotonic() - state["started"] > 3:
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            play_world = editor.get_game_world()
            layout = unreal.GameplayStatics.get_all_actors_of_class(play_world, unreal.MiningTunnelGenerator)[0]
            assert layout.hollow_region_prototype and layout.segment_triangle_budget == 30000
            assert layout.generated_fill.chunks[0].mesh.get_material(0).get_scalar_parameter_value("GuideCount") == 6
            state["runtime_started"] = time.time()
            runpy.run_path(str(scripts / "verify_direct_faces_runtime.py"), init_globals={"targets_override": targets, "runtime_output": "hollow_region_traversal.json"})
            state["phase"] = 2
        elif state["phase"] == 2:
            result_file = output / "hollow_region_traversal.json"
            if result_file.exists() and result_file.stat().st_mtime >= state["runtime_started"]:
                result = json.loads(result_file.read_text(encoding="utf-8"))
                assert result["passed"], result.get("reason")
                report["successful_cuts"] = result["successful_cuts"]
                report["rooms_reached"] = result["rooms_reached"]
                play_world = editor.get_game_world()
                layout = unreal.GameplayStatics.get_all_actors_of_class(play_world, unreal.MiningTunnelGenerator)[0]
                pawn = unreal.GameplayStatics.get_player_character(play_world, 0)
                pawn.get_component_by_class(unreal.MiningComponent).reset_field()
                assert all(segment.successful_cuts == 0 and segment.largest_chunk_triangles <= 30000 for segment in layout.generated_segments)
                assert layout.current_room_index == 0
                report["reset_restores_progression_and_budget"] = True
                finish(True, "Actual CharacterMovement and TryMine crossed the hollow path and three plugs; destination progression and reset passed")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
