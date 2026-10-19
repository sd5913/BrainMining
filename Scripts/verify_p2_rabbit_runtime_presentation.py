"""Exercise the real P2 encounter and verify hidden entry, free fall and seated pose."""
import json
import pathlib
import time
import traceback
import unreal

ROOT = pathlib.Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved" / "Automation" / "CountrysideP2"
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = "/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level(LEVEL)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = {"began": time.monotonic(), "commanded": False, "drop_seen": False, "settled_seen": False, "checks": []}

def finish():
    state["passed"] = bool(state["checks"]) and all(x["pass"] for x in state["checks"])
    (OUT / "rabbit_runtime_presentation.json").write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")
    if editor.get_game_world():
        levels.editor_request_end_play()
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.log("P2_RABBIT_RUNTIME_PRESENTATION " + str(state["passed"]))

def tick(_dt):
    try:
        now = time.monotonic()
        if now - state["began"] > 75:
            state["checks"].append({"label": "runtime presentation completed before timeout", "pass": False})
            finish()
            return
        world = editor.get_game_world()
        if not world:
            if not state["commanded"] and now - state["began"] > 2:
                levels.editor_request_begin_play()
            return
        stages = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CardRoomStage)
        if not stages:
            return
        stage = stages[0]
        rabbit = next((c for c in stage.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "RabbitOpponent"), None)
        seated = next((c for c in stage.get_components_by_class(unreal.PoseableMeshComponent) if c.get_name() == "RabbitSeatedOpponent"), None)
        if not rabbit or not seated:
            return
        age = now - state["began"]
        if not state["commanded"] and age > 4:
            state["checks"].append({"label": "rabbit hidden before encounter starts", "pass": not rabbit.is_visible() and not seated.is_visible()})
            unreal.SystemLibrary.execute_console_command(world, "BrainMining.ReviewP2")
            state["commanded"] = True
            state["encounter_at"] = now
            return
        if state["commanded"] and rabbit.is_simulating_physics() and not state["drop_seen"]:
            state["drop_seen"] = True
            state["checks"].append({"label": "rabbit enters true physics free fall", "pass": rabbit.is_simulating_physics()})
        if state["commanded"] and seated.is_visible() and not rabbit.is_visible() and not state["settled_seen"]:
            state["settled_seen"] = True
            state["checks"].append({"label": "rabbit switches to seated pose after landing", "pass": seated.is_visible() and not rabbit.is_visible()})
            state["checks"].append({"label": "rabbit uses character scale and physics asset", "pass": float(stage.get_editor_property("rabbit_visual_scale")) >= 10.0 and bool(stage.get_editor_property("rabbit_skeletal_mesh").get_editor_property("physics_asset"))})
            finish()
    except Exception:
        state["error"] = traceback.format_exc()
        finish()

handle = unreal.register_slate_post_tick_callback(tick)
