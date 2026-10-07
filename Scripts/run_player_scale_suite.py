import json
import runpy
import time
import traceback
from pathlib import Path

import unreal


scripts = Path(__file__).parent
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
queue = {"phase": 0, "started": time.time(), "phase_started": time.time()}


def run_script(filename, parameters=None):
    queue["phase_started"] = time.time()
    runpy.run_path(str(scripts / filename), init_globals=parameters)


def read_result(filename):
    path = output / filename
    if path.exists() and path.stat().st_mtime >= queue["phase_started"]:
        result = json.loads(path.read_text(encoding="utf-8"))
        assert result["passed"], result.get("reason", result.get("error"))
        return result
    return None


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(queue_callback)
    result = {"passed": passed, "reason": reason, "phase": queue["phase"], "elapsed": time.time() - queue["started"]}
    (output / "player_scale_suite.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("PLAYER_SCALE_SUITE " + json.dumps(result))


def run_queue(delta):
    try:
        if time.time() - queue["started"] > 480:
            finish(False, "Player scale test queue timed out")
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        if queue["phase"] == 0:
            run_script("verify_player_scale.py")
            queue["phase"] = 1
        elif queue["phase"] == 1 and read_result("player_scale.json"):
            run_script("verify_direct_faces_runtime.py", {"runtime_output": "player_scale_traversal.json"})
            queue["phase"] = 2
        elif queue["phase"] == 2 and read_result("player_scale_traversal.json"):
            run_script("verify_direct_faces_reset.py", {"runtime_output": "player_scale_traversal.json", "reset_output": "player_scale_reset.json"})
            read_result("player_scale_reset.json")
            queue["phase"] = 3
            finish(True, "Scale variants, tuning, actual 70% player traversal and full reset passed")
    except Exception:
        finish(False, traceback.format_exc())


queue_callback = unreal.register_slate_post_tick_callback(run_queue)
unreal.log("PLAYER_SCALE_SUITE_ARMED")
