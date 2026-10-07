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
    (output / "chunk_runtime_validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("CHUNK_RUNTIME_VALIDATION " + json.dumps(report))


def tick(delta):
    try:
        if time.time() - state["started"] > 600:
            finish(False, "Validation timed out")
            return
        if state["phase"] == 0 and time.time() - state["started"] > 3:
            world = editor.get_editor_world()
            generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
            assert generator.automatic_chunks and generator.simplify_chunks
            assert all(segment.chunked for segment in generator.generated_segments)
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            run_script("verify_chunk_async.py")
            state["phase"] = 2
        elif state["phase"] == 2 and result("chunk_async.json"):
            run_script("verify_segmented_lifecycle.py")
            state["phase"] = 6
        elif state["phase"] == 6 and result("segmented_lifecycle.json"):
            run_script("verify_direct_faces_runtime.py", {"runtime_output": "chunk_traversal.json"})
            state["phase"] = 3
        elif state["phase"] == 3 and result("chunk_traversal.json"):
            run_script("verify_direct_faces_reset.py", {"runtime_output": "chunk_traversal.json", "reset_output": "chunk_reset.json"})
            levels.editor_request_end_play()
            state["phase"] = 4
        elif state["phase"] == 4 and editor.get_game_world() is None:
            run_script("verify_auto_rooms_editor.py")
            state["phase"] = 5
        elif state["phase"] == 5 and result("auto_rooms_editor.json"):
            finish(True, "Chunk lifecycle, collision, real player traversal, reset and room-only editing passed")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("CHUNK_RUNTIME_VALIDATION_ARMED")
