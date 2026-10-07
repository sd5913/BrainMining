import json
import time
import traceback
from pathlib import Path

import unreal


saved = Path(unreal.Paths.project_saved_dir()) / "Validation"
layout = json.loads((saved / "editable_descent_layout.json").read_text(encoding="utf-8"))
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
if world is None or "Lvl_MiningDescent_Editable" not in world.get_path_name():
    raise RuntimeError("Start PIE in Lvl_MiningDescent_Editable first")
pawn = unreal.GameplayStatics.get_player_character(world, 0)
if pawn is None:
    raise RuntimeError("No player character in PIE")
mining = pawn.get_component_by_class(unreal.MiningComponent)
mining.reset_field()
controller = unreal.GameplayStatics.get_player_controller(world, 0)
camera = pawn.get_component_by_class(unreal.CameraComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
rocks = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock), key=lambda actor: actor.get_actor_location().x)
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
state = {"started": time.monotonic(), "next_cut": 0, "next_jump": 0, "jumps": 0, "phase": "walk", "max_x": pawn.get_actor_location().x, "rooms_reached": [], "max_cut_ms": 0, "passed": False}
last_progress = time.monotonic()


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    state["passed"] = passed
    state["reason"] = reason
    state["elapsed"] = time.monotonic() - state["started"]
    (saved / "editable_descent_runtime.json").write_text(json.dumps(state, indent=2), encoding="utf-8")
    unreal.log("EDITABLE_DESCENT_RUNTIME " + json.dumps(state))


def tick(delta):
    global last_progress
    try:
        if editor.get_game_world() is None:
            finish(False, "PIE stopped")
            return
        now = time.monotonic()
        position = pawn.get_actor_location()
        state["position"] = [position.x, position.y, position.z]
        if now - state["started"] > 100:
            finish(False, "Timed out")
            return
        if position.x > state["max_x"] + 5:
            state["max_x"] = position.x
            last_progress = now
        if now - last_progress > 14:
            state["mining_status"] = mining.status
            finish(False, "Player stopped progressing")
            return
        pitch = 0
        for entry, rock in zip(layout["rocks"], rocks):
            if position.x < rock.get_actor_location().x + 700 and position.x > entry["entrance"][0] - 380:
                pitch = -entry["slope"]
                break
        aim = unreal.Rotator(pitch=pitch, yaw=0, roll=0)
        if now - last_progress > 0.5:
            sweep = unreal.SystemLibrary.capsule_trace_single(world, position, position + unreal.Vector(100, 0, 0), capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if sweep:
                hit = sweep.to_tuple()
                state["last_obstacle"] = str(hit[9])
                if isinstance(hit[9], unreal.DiggableRock):
                    aim = unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), hit[5] - hit[7] * 5)
        controller.set_control_rotation(aim)
        if now - last_progress > 1.5 and now >= state["next_jump"]:
            pawn.jump()
            state["next_jump"] = now + 1
            state["jumps"] += 1
        else:
            pawn.stop_jumping()
        pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True)
        if now >= state["next_cut"]:
            mining.try_mine()
            state["next_cut"] = now + 0.34
        for rock in rocks:
            state["max_cut_ms"] = max(state["max_cut_ms"], rock.last_total_milliseconds)
        for section in layout["sections"]:
            if section["kind"] == "Room" and section["name"] != "00_START" and position.x > section["origin"][0] + 160 and section["name"] not in state["rooms_reached"]:
                state["rooms_reached"].append(section["name"])
        if position.x >= layout["finish"][0]:
            state["cuts_per_segment"] = [rock.successful_cuts for rock in rocks]
            assert len(state["rooms_reached"]) == 3
            assert all(value > 0 for value in state["cuts_per_segment"])
            mining.reset_field()
            state["reset_restores_all_rocks"] = all(rock.triangle_count == 12 for rock in rocks)
            state["reset_restores_player"] = pawn.get_actor_location().x < 300
            assert state["reset_restores_all_rocks"] and state["reset_restores_player"]
            finish(True, "Real CharacterMovement with mining and normal jumping traversed all three slopes, entered three rooms, and reset safely")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("EDITABLE_DESCENT_RUNTIME_ARMED")
