import json
import math
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None and "Lvl_MiningDescent_AutoRooms" in world.get_path_name(), "Start PIE first"
pawn = unreal.GameplayStatics.get_player_character(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
rock = generator.generated_fill
bedrock = generator.generated_bedrock
initial_triangles = rock.triangle_count
mining.reset_field()
start = pawn.get_actor_location()
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
report = {"passed": False, "rooms_reached": [], "links": [], "jumps": 0, "max_cut_ms": 0, "initial_triangles": initial_triangles}
report["lights"] = [{"hidden": room.get_editor_property("hidden"), "light_hidden": room.room_light.get_editor_property("hidden_in_game"), "visible": room.room_light.get_editor_property("visible"), "intensity": room.room_light.intensity, "units": str(room.room_light.intensity_units), "position": str(room.room_light.get_world_location())} for room in rooms]
state = {"started": time.monotonic(), "progress_time": time.monotonic(), "best_distance": 1e9, "target": 1, "next_cut": 0, "next_jump": 0, "revision": generator.build_revision}


def trace(first, second, ignored):
    return unreal.SystemLibrary.line_trace_single(world, first, second, unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignored, unreal.DrawDebugTrace.NONE, True)


def floor(room):
    center = room.get_actor_location()
    return center - unreal.Vector(0, 0, room.room_size.z * abs(room.get_actor_scale3d().z) * 0.5)


def anchor(first, second):
    first_floor, second_floor = floor(first), floor(second)
    horizontal = second_floor - first_floor
    horizontal.z = 0
    horizontal = horizontal / horizontal.length()
    distances = []
    for room in (first, second):
        scale = room.get_actor_scale3d()
        distances.append(min(room.room_size.x * abs(scale.x) / (2 * abs(horizontal.x)) if abs(horizontal.x) > 0.0001 else 1e12, room.room_size.y * abs(scale.y) / (2 * abs(horizontal.y)) if abs(horizontal.y) > 0.0001 else 1e12))
    return first_floor + horizontal * distances[0], second_floor - horizontal * distances[1]


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    pawn.stop_jumping()
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report["passed"] = passed
    report["reason"] = reason
    report["elapsed"] = time.monotonic() - state["started"]
    report["position"] = str(pawn.get_actor_location())
    report["mining_status"] = mining.status
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "auto_rooms_runtime.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AUTO_ROOMS_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        now = time.monotonic()
        if editor.get_game_world() is None:
            finish(False, "PIE stopped")
            return
        if not report["links"]:
            for first, second in zip(rooms, rooms[1:]):
                entrance, exit_point = anchor(first, second)
                along = (exit_point - entrance) / (exit_point - entrance).length()
                sideways = unreal.Vector(-along.y, along.x, 0)
                sideways = sideways / sideways.length()
                up = along.cross(sideways)
                middle = (entrance + exit_point) * 0.5 + up * 180
                fixed_sides = [trace(middle, middle + offset, [pawn, rock]) for offset in (sideways * 600, sideways * -600, up * 600, up * -450)]
                door_hit = trace(entrance - along * 80 + up * 180, exit_point + along * 80 + up * 180, [pawn, rock])
                link = {"fixed_sides": [hit is not None and hit.to_tuple()[9] == bedrock for hit in fixed_sides], "sealed_by_fill": trace(entrance - along * 100 + up * 180, exit_point + along * 100 + up * 180, [pawn]) is not None, "bedrock_does_not_block_door": door_hit is None, "door_hit": str(door_hit.to_tuple()) if door_hit else None}
                report["links"].append(link)
                assert all(link["fixed_sides"]) and link["sealed_by_fill"] and link["bedrock_does_not_block_door"], link
            for room in rooms:
                center = room.get_actor_location()
                assert trace(center - unreal.Vector(80, 0, 0), center + unreal.Vector(80, 0, 0), [pawn]) is None
                assert trace(center, floor(room) - unreal.Vector(0, 0, 150), [pawn]) is not None
                size = room.room_size
                scale = room.get_actor_scale3d()
                for offset in (unreal.Vector(size.x * abs(scale.x) * 0.5 - 40, 0, 0), unreal.Vector(-size.x * abs(scale.x) * 0.5 + 40, 0, 0), unreal.Vector(0, size.y * abs(scale.y) * 0.5 - 40, 0), unreal.Vector(0, -size.y * abs(scale.y) * 0.5 + 40, 0)):
                    floor_hit = trace(center + offset, floor(room) + offset - unreal.Vector(0, 0, 400), [pawn, rock])
                    assert floor_hit is not None and abs(floor_hit.to_tuple()[5].z - floor(room).z) < 1, "Tunnel extension cut a pit into a room floor"
            report["empty_rooms_solid_floors"] = True
            report["fall_reset_distance"] = mining.fall_reset_distance
            assert mining.fall_reset_distance >= 2300
            assert not bedrock.get_editor_property("hidden")
            report["bedrock_visible_in_game"] = True
        assert generator.build_revision == state["revision"], "Editor generation ran during play"
        if now - state["started"] > 140 or now - state["progress_time"] > 18:
            finish(False, "Character stopped progressing")
            return
        position = pawn.get_actor_location()
        target = rooms[state["target"]].get_actor_location()
        horizontal = target - position
        horizontal.z = 0
        distance = horizontal.length()
        if distance < state["best_distance"] - 5:
            state["best_distance"] = distance
            state["progress_time"] = now
        if distance < 140:
            report["rooms_reached"].append(rooms[state["target"]].connection_order)
            if state["target"] == len(rooms) - 1:
                report["successful_cuts"] = rock.successful_cuts
                report["no_runtime_regeneration"] = generator.build_revision == state["revision"]
                mining.reset_field()
                report["reset_restores_network"] = rock.triangle_count == initial_triangles
                report["reset_restores_player"] = (pawn.get_actor_location() - start).length() < 20
                assert report["reset_restores_network"] and report["reset_restores_player"]
                finish(True, "Actual CharacterMovement and TryMine traversed every generated connection and reset successfully")
                return
            state["target"] += 1
            state["best_distance"] = 1e9
            state["progress_time"] = now
            return
        direction = horizontal / distance
        entrance, exit_point = anchor(rooms[state["target"] - 1], rooms[state["target"]])
        slope = math.degrees(math.atan2(exit_point.z - entrance.z, math.hypot(exit_point.x - entrance.x, exit_point.y - entrance.y)))
        aim = unreal.Rotator(pitch=slope, yaw=math.degrees(math.atan2(direction.y, direction.x)))
        if now - state["progress_time"] > 0.5:
            obstacle = unreal.SystemLibrary.capsule_trace_single(world, position, position + direction * 100, capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if obstacle:
                hit = obstacle.to_tuple()
                report["last_obstacle"] = str(hit[9])
                if isinstance(hit[9], unreal.DiggableRock):
                    aim = unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), hit[5] - hit[7] * 5)
        controller.set_control_rotation(aim)
        if now - state["progress_time"] > 1.2 and now >= state["next_jump"]:
            pawn.jump()
            state["next_jump"] = now + 1
            report["jumps"] += 1
        else:
            pawn.stop_jumping()
        pawn.add_movement_input(direction, 1.0, True)
        if now >= state["next_cut"]:
            mining.try_mine()
            state["next_cut"] = now + 0.34
        report["max_cut_ms"] = max(report["max_cut_ms"], rock.last_total_milliseconds)
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("AUTO_ROOMS_RUNTIME_ARMED")
