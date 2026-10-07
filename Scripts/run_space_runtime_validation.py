import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


scripts = Path(__file__).parent
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
levels.editor_set_viewport_realtime(True)
state = {"phase": 0, "started": time.time(), "phase_started": time.time()}


def run_script(name, variables=None):
    state["phase_started"] = time.time()
    runpy.run_path(str(scripts / name), init_globals=variables or {})


def result(name):
    path = output / name
    if path.exists() and path.stat().st_mtime >= state["phase_started"]:
        data = json.loads(path.read_text(encoding="utf-8"))
        assert data["passed"], data.get("reason", data.get("error"))
        return data
    return None


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()
    report = {"passed": passed, "reason": reason, "phase": state["phase"], "elapsed": time.time() - state["started"]}
    (output / "space_runtime_validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_RUNTIME_VALIDATION " + json.dumps(report))


def tick(delta):
    try:
        if time.time() - state["started"] > 600:
            finish(False, "Validation timed out")
            return
        if state["phase"] == 0 and time.time() - state["started"] > 3:
            world = editor.get_editor_world()
            generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
            assert generator.rebuild_tunnels(), generator.last_build_status
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            run_script("verify_direct_faces_runtime.py", {"runtime_output": "space_main_regression.json"})
            state["phase"] = 2
        elif state["phase"] == 2 and result("space_main_regression.json"):
            run_script("verify_direct_faces_reset.py", {"runtime_output": "space_main_regression.json", "reset_output": "space_main_reset.json"})
            levels.editor_request_end_play()
            state["phase"] = 3
        elif state["phase"] == 3 and editor.get_game_world() is None:
            assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
            world = editor.get_editor_world()
            generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
            assert generator.rebuild_tunnels(), generator.last_build_status
            levels.editor_request_begin_play()
            state["phase"] = 4
        elif state["phase"] == 4 and editor.get_game_world() is not None:
            run_script("verify_space_runtime.py")
            state["phase"] = 5
        elif state["phase"] == 5 and result("space_traversal.json"):
            world = editor.get_game_world()
            generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
            initial = result("space_traversal.json")["initial_triangles"]
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            pawn.get_component_by_class(unreal.MiningComponent).reset_field()
            assert [segment.triangle_count for segment in generator.generated_segments] == initial
            assert generator.current_room_index == 0 and all(segment.successful_cuts == 0 for segment in generator.generated_segments)
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(4500, 1500, 200), unreal.Vector(4500, 2300, 200), unreal.TraceTypeQuery.ECC_VISIBILITY, True, [pawn], unreal.DrawDebugTrace.NONE, True)
            assert hit is not None, "Reset must restore hidden entrance wall"
            (output / "space_demo_reset.json").write_text(json.dumps({"passed": True, "initial_triangles": initial, "hidden_wall_restored": True}), encoding="utf-8")
            levels.editor_request_end_play()
            state["phase"] = 6
        elif state["phase"] == 6 and editor.get_game_world() is None:
            run_script("verify_space_editor.py")
            state["phase"] = 7
        elif state["phase"] == 7 and result("space_editor.json"):
            assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
            finish(True, "Original layout traversal/reset, hidden/open caves, main-room progression, demo reset, automatic editor updates and undo/redo passed")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
