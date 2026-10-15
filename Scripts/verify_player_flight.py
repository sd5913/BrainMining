import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None, "Start PIE on the hollow-branches demo first; this script never loads or saves maps."
pawn = unreal.GameplayStatics.get_player_character(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
movement = pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
mining = pawn.get_component_by_class(unreal.MiningComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = generator.hollow_region
assert region is not None
input_systems = [item for item in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem) if item.get_path_name().startswith("/Engine/Transient.")]
assert len(input_systems) == 1
inputs = input_systems[0]
jump_action = pawn.get_editor_property("jump_action")
move_action = pawn.get_editor_property("move_action")
minimap = controller.get_hud().get_component_by_class(unreal.MiningDeveloperMapComponent)
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "player_flight_runtime.json"
state = {"phase": 0, "elapsed": 0.0, "held": False, "move": unreal.Vector(), "started": time.monotonic(), "revision": generator.build_revision}
report = {"passed": False, "checks": [], "samples": {}, "scope": "Real Enhanced Input actions and CharacterMovement in PIE. Fixture staging teleports only between independent checks; no collision disabling, runtime terrain rebuild, or asset writes."}
half_height = capsule.get_scaled_capsule_half_height()
radius = capsule.get_scaled_capsule_radius()
room = region.from_room
floor = room.get_actor_location()
ceiling = floor.z + room.room_size.z
wall_y = floor.y + room.room_size.y * 0.5
mining.reset_field()
controller.set_control_rotation(unreal.Rotator())
spawn = pawn.get_actor_location()


def check(label, condition):
    assert condition, label
    report["checks"].append(label)


def sample(label):
    position = pawn.get_actor_location()
    velocity = movement.velocity
    report["samples"][label] = {"position": [position.x, position.y, position.z], "velocity": [velocity.x, velocity.y, velocity.z], "held": movement.flight_input_held, "mode": str(movement.movement_mode)}


def advance():
    state["phase"] += 1
    state["elapsed"] = 0.0


def stage(position):
    state["held"] = False
    state["move"] = unreal.Vector()
    movement.set_flight_input_held(False)
    movement.stop_movement_immediately()
    pawn.set_actor_location(position, False, True)
    movement.set_movement_mode(unreal.MovementMode.MOVE_FALLING)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    state["held"] = False
    if unreal.GameplayStatics.get_player_pawn(world, 0) is None:
        controller.possess(pawn)
    movement.flight_settings = generator.flight_settings
    controller.reset_ignore_move_input()
    mining.reset_field()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"], "phase": state["phase"], "no_runtime_regeneration": generator.build_revision == state["revision"], "max_triangles": generator.segment_triangle_budget})
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PLAYER_FLIGHT_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        assert editor.get_game_world() is not None
        assert generator.build_revision == state["revision"], "Unexpected runtime terrain regeneration"
        if time.monotonic() - state["started"] > 120:
            finish(False, "Timed out")
            return
        state["elapsed"] += min(delta, 0.1)
        elapsed = state["elapsed"]
        phase = state["phase"]
        position = pawn.get_actor_location()
        if phase == 0 and elapsed > 0.3:
            check("Flight enabled by existing generator without asset migration", movement.flight_settings.enabled)
            check("Player scale remains 0.7", abs(pawn.applied_player_scale - 0.7) < 0.001)
            state["held"] = True
            advance()
        elif phase == 1 and elapsed > 1.0:
            sample("held_one_second")
            check("Held jump action produces sustained ascent", position.z > spawn.z + 200 and movement.velocity.z > 300)
            check("Boost ascent speed follows player scale", movement.velocity.z <= movement.flight_settings.ascent_speed * movement.flight_settings.boost_speed_multiplier * movement.flight_size_scale + 1)
            state["release_z"] = position.z
            state["held"] = False
            advance()
        elif phase == 2 and elapsed > 0.95:
            sample("released")
            check("Release generates Completed and falls after coasting", not movement.flight_input_held and movement.velocity.z < 0 and position.z < state["release_z"])
            state["held"] = True
            advance()
        elif phase == 3 and elapsed > 0.9:
            sample("caught_fall")
            check("Repress arrests descent and resumes ascent", movement.velocity.z > 100)
            state["held"] = False
            advance()
        elif phase == 4 and elapsed > 2.0:
            sample("landed")
            check("Release lands normally without falling through floor", movement.movement_mode == unreal.MovementMode.MOVE_WALKING and abs(position.z - spawn.z) < 5)
            state["held"] = True
            advance()
        elif phase == 5 and elapsed > 4.0:
            sample("ceiling")
            check("Sustained thrust is blocked by the ceiling", position.z + half_height <= ceiling + 3 and position.z > ceiling - half_height - 10 and abs(movement.velocity.z) < 1)
            state["held"] = False
            advance()
        elif phase == 6 and elapsed > 0.8:
            check("Release detaches from ceiling instead of sticking", position.z < ceiling - half_height - 10 and movement.velocity.z < 0)
            mining.reset_field()
            state["held"] = True
            advance()
        elif phase == 7 and elapsed > 0.5:
            check("Can take off again after reset", movement.flight_input_held and position.z > spawn.z + 50)
            mining.reset_field()
            advance()
        elif phase == 8 and elapsed > 0.3:
            check("Reset clears thrust even while input remains held", not movement.flight_input_held and abs(position.z - spawn.z) < 5)
            state["held"] = False
            advance()
        elif phase == 9 and elapsed > 0.1:
            state["held"] = True
            state["move"] = unreal.Vector(1, 0, 0)
            advance()
        elif phase == 10 and elapsed > 1.0:
            sample("air_strafe")
            check("Airborne movement action controls horizontal travel", position.y > spawn.y + 150)
            check("Horizontal air speed uses scaled 500 cm/s setting", 340 < movement.velocity.y < 351)
            advance()
        elif phase == 11 and elapsed > 2.0:
            sample("side_wall")
            check("Airborne strafe cannot cross room bedrock", position.y + radius <= wall_y + 3 and position.y > wall_y - radius - 10)
            controller.set_control_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0))
            state["move"] = unreal.Vector()
            advance()
        elif phase == 12 and elapsed > 0.1:
            check("Bedrock cannot be mined while flying", not mining.try_mine())
            controller.un_possess()
            check("Unpossessing clears held flight input", not movement.flight_input_held)
            controller.possess(pawn)
            mining.reset_field()
            controller.set_control_rotation(unreal.Rotator())
            state["held"] = False
            settings = movement.flight_settings
            settings.enabled = False
            movement.flight_settings = settings
            advance()
        elif phase == 13 and elapsed > 0.2:
            state["held"] = True
            advance()
        elif phase == 14 and elapsed > 0.2:
            sample("ordinary_jump")
            check("Disabling flight restores ordinary jump", not movement.flight_input_held and position.z > spawn.z + 10)
            advance()
        elif phase == 15 and elapsed > 2.0:
            check("Disabled mode does not fly forever while held", movement.movement_mode == unreal.MovementMode.MOVE_WALKING)
            movement.flight_settings = generator.flight_settings
            plug = next(item for item in region.plugs if item.enabled)
            distance = plug.position * region.path.get_spline_length()
            before = region.path.get_location_at_distance_along_spline(distance - plug.thickness * 0.5 - 160, unreal.SplineCoordinateSpace.WORLD)
            stage(before)
            target = region.path.get_location_at_distance_along_spline(distance, unreal.SplineCoordinateSpace.WORLD)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(before, target))
            advance()
        elif phase == 16 and elapsed > 0.12:
            state["held"] = True
            advance()
        elif phase == 17 and elapsed > 0.25:
            sample("mining_in_flight")
            check("Ordinary rock remains mineable during ascent", movement.flight_input_held and movement.velocity.z > 0 and mining.try_mine())
            check("Developer map remains tied to generator revision", minimap.visible and minimap.snapshot_revision == generator.build_revision)
            report["minimap_label"] = minimap.location_label
            report["mined_cuts"] = sum(segment.successful_cuts for segment in generator.generated_segments)
            destination = region.to_room
            approach = destination.get_actor_location() + unreal.Vector(-destination.room_size.x * 0.5 - 80, 0, 250)
            stage(approach)
            controller.set_control_rotation(unreal.Rotator())
            advance()
        elif phase == 18 and elapsed > 0.15:
            state["held"] = True
            state["move"] = unreal.Vector(0, 1, 0)
            advance()
        elif phase == 19 and elapsed > 1.0:
            sample("arrival_in_flight")
            check("Flying into next room still advances progression", generator.current_room_index == 1)
            check("Whole previous region still freezes on arrival", all(segment.segment_state == unreal.MiningSegmentState.FROZEN for segment in generator.generated_segments))
            report["largest_chunks"] = [segment.largest_chunk_triangles for segment in generator.generated_segments]
            state["held"] = False
            state["move"] = unreal.Vector()
            mining.reset_field()
            advance()
        elif phase == 20 and elapsed > 0.3:
            check("Reset restores room progression", generator.current_room_index == 0)
            check("Reset keeps existing scale", abs(pawn.applied_player_scale - 0.7) < 0.001)
            finish(True, "All flight, collision, mining, map, progression, reset and legacy-jump checks passed")
            return
        inputs.inject_input_vector_for_action(jump_action, unreal.Vector(1 if state["held"] else 0, 0, 0), [], [])
        inputs.inject_input_vector_for_action(move_action, state["move"], [], [])
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
