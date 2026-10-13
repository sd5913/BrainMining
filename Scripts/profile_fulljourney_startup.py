import json
import time
import traceback
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_saved_dir())
output = root / "Performance" / "fulljourney-startup.json"
output.parent.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = {"phase": "start", "started": time.perf_counter(), "frames_ms": []}
result = {"mode": "NullRHI PIE; no GPU or tool effect measurement"}


def finish(error=None):
    if error:
        result["error"] = error
    result["frames_ms"] = state["frames_ms"]
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    unreal.log("FULLJOURNEY_STARTUP_REPORT " + str(output))
    unreal.SystemLibrary.quit_editor()


def tick(delta_seconds):
    now = time.perf_counter()
    try:
        if now - state["started"] > 80:
            finish("Startup/first-cut probe exceeded 80 seconds")
            return
        if state["phase"] == "start":
            world = editor.get_editor_world()
            if "Lvl_FullJourney" not in world.get_path_name():
                finish("Expected FullJourney editor world")
                return
            state.update(phase="playing", requested=now)
            levels.editor_request_begin_play()
            return
        world = editor.get_game_world()
        if not world:
            return
        if state["phase"] == "playing":
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property("bReady"):
                return
            result["play_ready_seconds"] = now - state["requested"]
            hall = halls[0]
            areas = hall.get_editor_property("areas")
            area_index = next(index for index, area in enumerate(areas) if str(area.tag) == "BM_Campaign_Mine01")
            for _ in range(len(areas)):
                if hall.get_editor_property("unlocked_area") >= area_index:
                    break
                if not hall.skip_current_room():
                    finish("Could not use campaign skip to reach first mine")
                    return
            if not hall.enter_area(area_index, True):
                finish("Could not activate first mine")
                return
            field = hall.get_editor_property("treasure")
            placements = field.get_editor_property("authored_placements")
            placement = next(item for item in placements if str(item.area_tag) == "BM_Campaign_Mine01" and item.hosts)
            state.update(phase="cut", placement=placement, field=field, last=time.perf_counter())
            return
        if state["phase"] == "cut":
            placement = state["placement"]
            origin = placement.surface.translation
            state["rocks"] = [rock for rock in placement.hosts if isinstance(rock, unreal.GeneratedMiningRock)]
            if not state["rocks"]:
                finish("First ore has no generated terrain host")
                return
            result["before_cuts"] = [rock.successful_cuts for rock in state["rocks"]]
            started = time.perf_counter()
            result["accepted"] = [rock.mine_sphere(origin, 100) for rock in state["rocks"]]
            result["first_request_ms"] = (time.perf_counter() - started) * 1000
            result["inline_cuts"] = [rock.successful_cuts for rock in state["rocks"]]
            state.update(phase="drain", cut_started=started, last=time.perf_counter())
            return
        state["frames_ms"].append((now - state["last"]) * 1000)
        state["last"] = now
        if any(rock.get_editor_property("bCutPending") for rock in state["rocks"]):
            return
        result["completion_ms"] = (now - state["cut_started"]) * 1000
        result["rocks"] = [dict(name=rock.get_name(), cuts=rock.successful_cuts,
            boolean_ms=rock.last_boolean_milliseconds, collision_ms=rock.last_collision_milliseconds,
            notification_ms=rock.last_terrain_notification_milliseconds, status=rock.last_status)
            for rock in state["rocks"]]
        if not any(result["accepted"]) or not any(rock.successful_cuts > before for rock, before in zip(state["rocks"], result["before_cuts"])):
            finish("First cut did not change terrain")
            return
        finish()
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
