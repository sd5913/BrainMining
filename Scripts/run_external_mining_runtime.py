"""Isolated PIE validation of saved geometry, actual movement, TryMine and R reset."""
import json
import time
import traceback
from pathlib import Path
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
state = {"started": time.monotonic(), "phase": 0}
report = {"passed": False}

def finish(passed, reason):
    report.update(passed=passed, reason=reason, elapsed_seconds=time.monotonic() - state["started"])
    (output / "external_mining_runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("EXTERNAL_MINING_RUNTIME " + json.dumps(report, ensure_ascii=False))
    levels.editor_request_end_play()
    state["phase"] = 9
    state["finish_time"] = time.monotonic()

def tick(delta):
    try:
        now = time.monotonic()
        if state["phase"] == 9:
            if editor.get_game_world() is None and now - state["finish_time"] > 2:
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), "QUIT_EDITOR")
            return
        if now - state["started"] > 90:
            finish(False, "PIE validation timed out")
            return
        if state["phase"] == 0 and now - state["started"] > 2:
            assert levels.load_level("/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo")
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1 and editor.get_game_world() is not None:
            world = editor.get_game_world()
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            if not pawn:
                return
            region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExternalRegion)[0]
            assert region.generated_rock.chunked and region.generated_shell
            assert not isinstance(region.generated_shell, unreal.DiggableRock)
            state.update(world=world, pawn=pawn, region=region, mining=pawn.get_component_by_class(unreal.MiningComponent),
                         initial_triangles=region.generated_rock.triangle_count, start_x=pawn.get_actor_location().x)
            pawn.get_controller().set_control_rotation(unreal.Rotator(0, 0, 0))
            state["phase"] = 2
        elif state["phase"] == 2:
            pawn = state["pawn"]
            position = pawn.get_actor_location()
            assert position.z > 300, "Player fell through cave floor"
            if position.x < 2700:
                pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0)
            else:
                assert position.x - state["start_x"] > 5000
                report["actual_character_walk_cm"] = position.x - state["start_x"]
                pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
                state["phase"] = 3
        elif state["phase"] == 3:
            pawn = state["pawn"]
            if pawn.get_actor_location().y < 150:
                pawn.add_movement_input(unreal.Vector(0, 1, 0), 1.0)
            else:
                pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
                # Aim at the nearby cave floor, within the existing mining reach.
                pawn.get_controller().set_control_rotation(unreal.Rotator(pitch=-70, yaw=0, roll=0))
                state["aim_time"] = now
                state["phase"] = 4
        elif state["phase"] == 4 and now - state["aim_time"] > 1:
            camera = unreal.GameplayStatics.get_player_camera_manager(state["world"], 0)
            report["mining_view"] = {"location": str(camera.get_camera_location()),
                                     "rotation": str(camera.get_camera_rotation()),
                                     "reach": state["mining"].reach,
                                     "pawn": str(state["pawn"].get_actor_location())}
            assert state["mining"].try_mine(), str(state["mining"].status)
            assert state["region"].generated_rock.successful_cuts > 0
            report["try_mine_successful_cuts"] = state["region"].generated_rock.successful_cuts
            state["mining"].reset_field()
            assert state["region"].generated_rock.successful_cuts == 0
            assert state["region"].generated_rock.triangle_count == state["initial_triangles"]
            assert abs(state["pawn"].get_actor_location().x - state["start_x"]) < 5
            report["reset_restores_spawn_and_initial_passage"] = True
            finish(True, "Saved cave traversed using CharacterMovement; current TryMine and R reset passed")
    except Exception:
        finish(False, traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
