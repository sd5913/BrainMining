import json
import time
import traceback
from pathlib import Path

import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
thin_wall = "-PickaxeThinWall" in unreal.SystemLibrary.get_command_line()
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "Pickaxe"
if thin_wall:
    output = output / "ThinWall"
output.mkdir(parents=True, exist_ok=True)
report = {"passed": False, "checks": [], "scope": "MiningLab PIE; real player, swing timing and debris"}
report["thin_wall"] = thin_wall
state = {"phase": "start", "started": time.monotonic(), "next": 0.0, "strike": 0}


def check(condition, label):
    if not condition:
        raise AssertionError(label)
    report["checks"].append(label)


def finish(error=None):
    report["passed"] = error is None
    report["error"] = error
    report["elapsed"] = time.monotonic() - state["started"]
    (output / "runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    levels.editor_request_end_play()
    unreal.log("PICKAXE_RUNTIME_REPORT " + json.dumps(report))
    state.update(phase="shutdown", shutdown_at=time.monotonic() + 1.0)


def aim():
    rock = state["rock"]
    origin = rock.get_actor_location()
    pawn = state["pawn"]
    pawn.set_actor_location(unreal.Vector(origin.x - rock.rock_size.x * 0.5 - 260, origin.y, origin.z), False, True)
    eye = pawn.get_actor_eyes_view_point()[0]
    state["controller"].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, origin))


def tick(delta):
    try:
        if state["phase"] == "shutdown":
            if editor.get_game_world() is None and time.monotonic() >= state["shutdown_at"]:
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.quit_editor()
            return
        if time.monotonic() - state["started"] > 100:
            raise TimeoutError("PIE validation timed out: " + state["phase"])
        phase = state["phase"]
        if phase == "start":
            check(levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningLab"), "Existing MiningLab loads")
            levels.editor_request_begin_play()
            state["phase"] = "setup"
            return
        if phase == "setup":
            world = editor.get_game_world()
            if not world:
                return
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            if not pawn:
                return
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            effects = pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
            rocks = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock)
            check(mining is not None and effects is not None and bool(rocks), "Real player has mining, tool effects and terrain")
            rock = rocks[0]
            rock.fog_chamber.set_editor_property("bEnabled", False)
            rock.rock_size = unreal.Vector(50 if thin_wall else 600, 800, 800)
            mining.reset_field()
            mining.interval = 0.65
            mining.radius = 100.0
            mining.downward_offset = 0.0
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state.update(world=world, pawn=pawn, mining=mining, effects=effects, rock=rock,
                         controller=unreal.GameplayStatics.get_player_controller(world, 0))
            aim()
            state.update(phase="hit", next=unreal.GameplayStatics.get_time_seconds(world) + 2.0)
            return
        now = unreal.GameplayStatics.get_time_seconds(state["world"])
        if now < state["next"]:
            return
        mining = state["mining"]
        effects = state["effects"]
        rock = state["rock"]
        if phase == "hit":
            state["before_cuts"] = rock.successful_cuts
            state["before_bursts"] = effects.debris_burst_count
            state["before_swings"] = effects.swing_count
            check(mining.try_mine(), "Player swing %d is accepted" % (state["strike"] + 1))
            check(rock.successful_cuts == state["before_cuts"], "Terrain stays unchanged when swing starts")
            check(effects.swing_count == state["before_swings"] + 1, "Each attempt starts one swing")
            check(not mining.try_mine(), "Cooldown rejects duplicate attempts")
            state.update(phase="impact", next=now + 0.32)
        elif phase == "impact":
            state["strike"] += 1
            strike = state["strike"]
            check(effects.debris_burst_count == state["before_bursts"] + 1, "Strike %d produces impact debris" % strike)
            if strike < 3:
                check(rock.successful_cuts == strike, "Strike %d carves a real intermediate cavity" % strike)
                check(mining.target_pickaxe_hits == strike, "Local damage advances to %d of 3" % strike)
                marks = rock.get_components_by_class(unreal.DynamicMeshComponent)
                check(len(marks) == 1, "Fractures are actual terrain geometry without crack overlays")
            else:
                check(rock.successful_cuts == 3, "Third impact completes three progressive excavations")
                check(mining.get_editor_property("bLastPickaxeBreak"), "Third impact uses breaking feedback")
                check(len(rock.get_components_by_class(unreal.DynamicMeshComponent)) == 1, "Broken rock has no floating damage marks")
            report.setdefault("stages", []).append(dict(strike=strike, cuts=rock.successful_cuts, triangles=rock.triangle_count))
            state.update(phase="capture", next=now + 0.6)
        elif phase == "capture":
            unreal.SystemLibrary.execute_console_command(state["world"], 'Shot filename="' + str(output / ("strike-%d.png" % state["strike"])) + '" -nosuffix', state["controller"])
            state.update(phase="hit" if state["strike"] < 3 else "cancel", next=now + 0.25)
        elif phase == "cancel":
            mining.reset_field()
            aim()
            state.update(phase="cancel_swing", next=now + 0.3)
        elif phase == "cancel_swing":
            check(mining.try_mine(), "New swing starts after reset")
            mining.cancel_mining()
            state["before_bursts"] = effects.debris_burst_count
            state.update(phase="check_cancel", next=now + 0.7)
        elif phase == "check_cancel":
            check(rock.successful_cuts == 0 and mining.target_pickaxe_hits == 0, "Cancelling a pending swing prevents damage")
            check(effects.debris_burst_count == state["before_bursts"], "Cancelled swing emits no impact debris")
            check(mining.try_mine(), "Swing can restart after cancellation")
            mining.reset_field()
            aim()
            state.update(phase="drill", next=now + 0.7)
        elif phase == "drill":
            check(rock.successful_cuts == 0 and mining.target_pickaxe_hits == 0, "Field reset also cancels pending impacts")
            check(mining.activate_drill(), "Existing drill still activates")
            check(mining.try_mine(), "Drill still cuts immediately")
            check(rock.successful_cuts == 1, "Drill does not inherit three-hit gating")
            state.update(phase="finish", next=now + 0.4)
        elif phase == "finish":
            mining.reset_field()
            finish()
    except Exception:
        finish(traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("PICKAXE_RUNTIME_VALIDATION_ARMED")
