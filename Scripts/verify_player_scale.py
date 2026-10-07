import json
import math
import traceback
from pathlib import Path

import unreal


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world is not None, "Start PIE first"
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
pawn = unreal.GameplayStatics.get_player_character(world, 0)
original_settings = generator.get_editor_property("player_scale_settings").copy()
original_revision = generator.build_revision
spawned = []
report = {"passed": False, "checks": 0, "variants": []}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "player_scale.json"


def near(actual, expected, name):
    assert math.isclose(actual, expected, rel_tol=0.0001, abs_tol=0.01), f"{name}: {actual} != {expected}"
    report["checks"] += 1


def snapshot(character):
    capsule = character.get_component_by_class(unreal.CapsuleComponent)
    movement = character.get_component_by_class(unreal.CharacterMovementComponent)
    mining = character.get_component_by_class(unreal.MiningComponent)
    camera = character.get_component_by_class(unreal.CameraComponent)
    position = character.get_actor_location()
    return {
        "scale": character.get_actor_scale3d().x,
        "applied": character.applied_player_scale,
        "radius": capsule.get_scaled_capsule_radius(),
        "half_height": capsule.get_scaled_capsule_half_height(),
        "unscaled_radius": capsule.get_unscaled_capsule_radius(),
        "unscaled_half_height": capsule.get_unscaled_capsule_half_height(),
        "feet_z": position.z - capsule.get_scaled_capsule_half_height(),
        "camera_above_center": camera.get_world_location().z - position.z,
        "meshes": [mesh.get_world_scale().x for mesh in character.get_components_by_class(unreal.SkeletalMeshComponent)],
        "speed": movement.max_walk_speed,
        "crouch_speed": movement.max_walk_speed_crouched,
        "acceleration": movement.max_acceleration,
        "braking": movement.braking_deceleration_walking,
        "step": movement.max_step_height,
        "jump_velocity": movement.jump_z_velocity,
        "gravity": movement.gravity_scale,
        "mining_radius": mining.radius,
        "reach": mining.reach,
        "offset": mining.downward_offset,
        "interval": mining.interval,
        "inset": mining.inset_fraction,
        "first_person_scale": camera.first_person_scale,
        "first_person_fov": camera.first_person_field_of_view,
    }


def spawn_with(settings):
    generator.player_scale_settings = settings
    transform = unreal.Transform(location=pawn.get_actor_location() + unreal.Vector(0, 5000, 0))
    gameplay = unreal.get_default_object(unreal.GameplayStatics)
    actor = gameplay.call_method("BeginDeferredActorSpawnFromClass", args=(
        world, pawn.get_class(), transform, unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,
        None, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT
    ))
    spawned.append(actor)
    gameplay.call_method("FinishSpawningActor", args=(actor, transform, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    return actor


try:
    owned_segments = [segment for segment in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock) if segment.get_owner() == generator]
    assert set(owned_segments) == set(generator.generated_segments), "Untracked overlapping generated rocks are active in PIE"
    baseline_settings = unreal.MiningPlayerScaleSettings()
    baseline_settings.enabled = False
    baseline = snapshot(spawn_with(baseline_settings))
    for scale in (1.0, 0.7, 0.5, 0.7, 0.25, 2.0):
        settings = unreal.MiningPlayerScaleSettings()
        settings.scale = scale
        character = spawn_with(settings)
        actual = snapshot(character)
        for key in ("scale", "radius", "half_height", "camera_above_center", "speed", "crouch_speed", "acceleration", "braking", "step", "jump_velocity", "gravity", "mining_radius", "reach", "offset"):
            near(actual[key], baseline[key] * scale, f"{scale}: {key}")
        for key in ("feet_z", "unscaled_radius", "unscaled_half_height", "interval", "inset", "first_person_scale", "first_person_fov"):
            near(actual[key], baseline[key], f"{scale}: preserved {key}")
        for actual_mesh, baseline_mesh in zip(actual["meshes"], baseline["meshes"]):
            near(actual_mesh, baseline_mesh * scale, f"{scale}: mesh")
        near(actual["applied"], scale, "applied scale")
        mining = character.get_component_by_class(unreal.MiningComponent)
        initial_position = character.get_actor_location()
        for reset in range(3):
            mining.reset_field()
            after = snapshot(character)
            for key in ("scale", "half_height", "speed", "gravity", "mining_radius", "reach"):
                near(after[key], actual[key], f"reset {reset}: {key}")
            near((character.get_actor_location() - initial_position).length(), 0, "reset position")
        report["variants"].append(actual)
        character.destroy_actor()
        spawned.remove(character)
    settings = unreal.MiningPlayerScaleSettings()
    settings.scale = 0.7
    settings.movement_speed_multiplier = 1.4
    settings.jump_height_multiplier = 1.5
    settings.step_height_multiplier = 1.2
    settings.mining_radius_multiplier = 1.25
    settings.mining_reach_multiplier = 1.5
    settings.mining_downward_offset_multiplier = 0.5
    tuned = snapshot(spawn_with(settings))
    for key, factor in (("speed", 0.98), ("jump_velocity", 0.7 * math.sqrt(1.5)), ("step", 0.84), ("mining_radius", 0.875), ("reach", 1.05), ("offset", 0.35), ("radius", 0.7), ("gravity", 0.7)):
        near(tuned[key], baseline[key] * factor, "independent tuning: " + key)
    report["independent_tuning"] = tuned
    baseline_again = snapshot(spawn_with(baseline_settings))
    for key in ("scale", "radius", "half_height", "speed", "jump_velocity", "gravity", "mining_radius", "reach", "camera_above_center"):
        near(baseline_again[key], baseline[key], "blueprint baseline unchanged: " + key)
    near(generator.build_revision, original_revision, "No terrain regeneration")
    generator.player_scale_settings = original_settings
    assert generator.player_scale_settings.enabled == original_settings.enabled
    near(generator.player_scale_settings.scale, original_settings.scale, "runtime config restored")
    mining = pawn.get_component_by_class(unreal.MiningComponent)
    original_player = snapshot(pawn)
    for reset in range(3):
        mining.reset_field()
        for key in ("scale", "radius", "half_height", "speed", "jump_velocity", "gravity", "mining_radius", "reach"):
            near(snapshot(pawn)[key], original_player[key], "real player reset: " + key)
    report["player"] = snapshot(pawn)
    report["baseline"] = baseline
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    generator.player_scale_settings = original_settings
    for character in spawned:
        character.destroy_actor()
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PLAYER_SCALE " + json.dumps(report))
