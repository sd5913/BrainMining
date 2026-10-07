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
state = {"phase": 0, "started": time.time(), "phase_started": time.time()}


def run_script(name):
    state["phase_started"] = time.time()
    runpy.run_path(str(scripts / name))


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
    (output / "player_scale_validation.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PLAYER_SCALE_VALIDATION " + json.dumps(report))


def tick(delta):
    try:
        if time.time() - state["started"] > 600:
            finish(False, "Validation timed out")
            return
        if state["phase"] == 0 and time.time() - state["started"] > 2:
            run_script("verify_player_scale_editor.py")
            state["phase"] = 1
        elif state["phase"] == 1 and result("player_scale_editor.json"):
            run_script("run_player_scale_suite.py")
            levels.editor_request_begin_play()
            state["phase"] = 2
        elif state["phase"] == 2 and result("player_scale_suite.json"):
            levels.editor_request_end_play()
            state["phase"] = 3
            state["phase_started"] = time.time()
        elif state["phase"] == 3 and editor.get_game_world() is None and time.time() - state["phase_started"] > 1:
            run_script("verify_auto_rooms_editor.py")
            state["phase"] = 4
        elif state["phase"] == 4 and result("auto_rooms_editor.json"):
            run_script("verify_player_scale_editor.py")
            state["phase"] = 5
        elif state["phase"] == 5 and result("player_scale_editor.json"):
            world = editor.get_editor_world()
            generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
            assert generator.player_scale_settings.enabled and abs(generator.player_scale_settings.scale - 0.7) < 0.0001
            assert world.get_outer() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
            finish(True, "Editor controls, 70% traversal, resets, room editing and final saved settings passed")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("PLAYER_SCALE_VALIDATION_ARMED")
