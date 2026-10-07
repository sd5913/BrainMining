import gc
import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
scripts = Path(unreal.Paths.project_dir()) / "Scripts"
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningShapes_Demo")
levels.editor_set_viewport_realtime(True)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
controls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelShapeSection)
assert len(controls) == 3
saved_regions = len(generator.generated_map_regions)
assert generator.rebuild_tunnels(), generator.last_build_status
assert len(generator.generated_map_regions) == saved_regions
assert all(section.tunnel in generator.routes for section in controls)
state = {"phase": 0, "started": time.monotonic(), "runtime_started": 0}
report = {"passed": False, "reload_controls": len(controls), "reload_regions": saved_regions}
gc.collect()


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (output / "tunnel_shape_runtime.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("TUNNEL_SHAPE_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        if time.monotonic() - state["started"] > 380:
            finish(False, "Validation timed out")
        elif state["phase"] == 0 and time.monotonic() - state["started"] > 3:
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            play_world = editor.get_game_world()
            layout = unreal.GameplayStatics.get_all_actors_of_class(play_world, unreal.MiningTunnelGenerator)[0]
            for section in unreal.GameplayStatics.get_all_actors_of_class(play_world, unreal.MiningTunnelShapeSection):
                assert section.get_editor_property("frame").get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
                assert section.get_editor_property("frame").get_editor_property("hidden_in_game")
            targets = []
            for route in layout.routes:
                targets.extend((route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD), None) for index in range(route.path.get_number_of_spline_points()))
                targets.append((route.to_room.get_actor_location(), route.to_room.connection_order))
            state["runtime_started"] = time.time()
            runpy.run_path(str(scripts / "verify_direct_faces_runtime.py"), init_globals={"targets_override": targets, "runtime_output": "tunnel_shape_traversal.json"})
            state["phase"] = 2
        elif state["phase"] == 2:
            result_file = output / "tunnel_shape_traversal.json"
            if result_file.exists() and result_file.stat().st_mtime >= state["runtime_started"]:
                result = json.loads(result_file.read_text(encoding="utf-8"))
                assert result["passed"], result.get("reason")
                report["successful_cuts"] = result["successful_cuts"]
                report["rooms_reached"] = result["rooms_reached"]
                play_world = editor.get_game_world()
                layout = unreal.GameplayStatics.get_all_actors_of_class(play_world, unreal.MiningTunnelGenerator)[0]
                pawn = unreal.GameplayStatics.get_player_character(play_world, 0)
                pawn.get_component_by_class(unreal.MiningComponent).reset_field()
                assert all(segment.successful_cuts == 0 for segment in layout.generated_segments)
                assert all(segment.largest_chunk_triangles <= 30000 for segment in layout.generated_segments)
                report["reset_preserves_budget"] = True
                finish(True, "Saved shape controls reload; actual CharacterMovement and TryMine traverse all shaped links; reset restores terrain")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
