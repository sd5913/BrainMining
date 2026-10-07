import json
import time
import traceback
from pathlib import Path
import unreal


settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
state = {"phase": "sealed", "started": time.monotonic(), "next_cut": 0.0, "seen_seep": False, "seen_breach": False, "entered_room": False, "max_puffs": 0}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "fog_chamber_runtime.json"


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    state["passed"] = passed
    state["reason"] = reason
    output.write_text(json.dumps(state, indent=2), encoding="utf-8")
    unreal.log("FOG_RUNTIME_RESULT " + json.dumps(state))


def tick(delta):
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:
            finish(False, "PIE stopped")
            return
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        mining = pawn.get_component_by_class(unreal.MiningComponent)
        rock = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock)[0]
        chamber = rock.get_component_by_class(unreal.FogChamberComponent)
        now = time.monotonic()
        if now - state["started"] > 50:
            finish(False, "Runtime validation timed out: " + str(pawn.get_actor_location()))
            return
        state["max_puffs"] = max(state["max_puffs"], chamber.active_puffs)
        assert chamber.active_puffs <= 192
        if state["phase"] == "sealed":
            assert chamber.room_ready and chamber.state == unreal.FogChamberState.SEALED
            assert rock.triangle_count == 24
            state["fresh_begin_play_sealed"] = True
            unreal.GameplayStatics.get_player_controller(world, 0).set_control_rotation(unreal.Rotator(pitch=0, yaw=0, roll=0))
            assert mining.try_mine()
            assert chamber.state == unreal.FogChamberState.SEEPING
            state["seen_seep"] = True
            state["phase"] = "seep"
            state["seep_time"] = now
        elif state["phase"] == "seep":
            assert chamber.breach_count == 0
            if now - state["seep_time"] > 2.5:
                state["phase"] = "walk"
        elif state["phase"] == "walk":
            pawn.add_movement_input(unreal.Vector(1, 0, 0), 1.0, True)
            if now >= state["next_cut"]:
                mining.try_mine()
                state["next_cut"] = now + 0.36
            if chamber.breach_count == 1:
                state["seen_breach"] = True
            position = pawn.get_actor_location()
            if position.x > 520:
                state["entered_room"] = True
            if position.x > 1250:
                state["exit_position"] = str(position)
                state["cuts"] = rock.successful_cuts
                state["phase"] = "settle"
        elif state["phase"] == "settle":
            if chamber.state == unreal.FogChamberState.OPEN:
                assert chamber.breach_count == 1 and chamber.pressure == 0
                state["pressure_decayed_to_open"] = True
                assert state["seen_seep"] and state["seen_breach"] and state["entered_room"]
                mining.reset_field()
                assert chamber.active_puffs == 0 and chamber.breach_count == 0
                assert chamber.state == unreal.FogChamberState.SEALED and rock.triangle_count == 24
                assert pawn.get_actor_location().x < 0
                state["reset_clears_particles_and_restores_room"] = True
                finish(True, "Real CharacterMovement entered and exited the fog chamber, then reset safely")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("FOG_RUNTIME_VALIDATION_ARMED")
