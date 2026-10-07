import json
import math
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None, "Start PIE first"
pawn = unreal.GameplayStatics.get_player_character(world, 0)
controller = unreal.GameplayStatics.get_player_controller(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
camera = pawn.get_component_by_class(unreal.CameraComponent)
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
source = generator
assert source.layout_applied
rock = generator.generated_fill
bedrock = generator.generated_bedrock
mining.reset_field()
initial_triangles = rock.triangle_count
start = pawn.get_actor_location()
targets = []
sections = []
for route in source.routes:
    route_points = [route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(route.path.get_number_of_spline_points())]
    targets.extend((point, None) for point in route_points)
    targets.append((route.to_room.get_actor_location(), route.to_room.connection_order))
    width = min(source.tunnel_width, route.from_room.room_size.x - source.door_margin * 2, route.from_room.room_size.y - source.door_margin * 2, route.to_room.room_size.x - source.door_margin * 2, route.to_room.room_size.y - source.door_margin * 2)
    for index in range(1, len(route_points)):
        first, second = route_points[index - 1], route_points[index]
        direction = unreal.Vector(second.x - first.x, second.y - first.y, 0)
        direction = direction / direction.length()
        ramp_start = first + direction * (width * 0.75 if index > 1 else 0)
        ramp_end = second - direction * (width * 0.75 if index < len(route_points) - 1 else 0)
        sections.append((ramp_start, ramp_end))
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
report = {"passed": False, "rooms_reached": [], "initial_triangles": initial_triangles, "waypoints": len(targets), "jumps": 0}
report["peak_triangles"] = initial_triangles
state = {"started": time.monotonic(), "progress_time": time.monotonic(), "best": 1e9, "target": 0, "next_cut": 0, "revision": generator.build_revision}


def trace(first, second, ignored):
    return unreal.SystemLibrary.line_trace_single(world, first, second, unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignored, unreal.DrawDebugTrace.NONE, True)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"], "position": str(pawn.get_actor_location()), "target_index": state["target"], "mining_status": mining.status})
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "route_runtime.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("ROUTE_RUNTIME " + json.dumps(report))


def tick(delta):
    try:
        now = time.monotonic()
        report["peak_triangles"] = max(report["peak_triangles"], rock.triangle_count)
        report["successful_cuts"] = rock.successful_cuts
        assert editor.get_game_world() is not None, "PIE stopped"
        assert generator.build_revision == state["revision"], "Runtime regeneration erased mining progress"
        if "shell_checks" not in report:
            report["shell_checks"] = 0
            for first, second in sections:
                direction = second - first
                horizontal = unreal.Vector(direction.x, direction.y, 0)
                horizontal = horizontal / horizontal.length()
                side = unreal.Vector(-horizontal.y, horizontal.x, 0)
                center = (first + second) * 0.5 + unreal.Vector(0, 0, 120)
                offsets = (side * 650, side * -650, unreal.Vector(0, 0, 650), unreal.Vector(0, 0, -400))
                for offset in offsets:
                    hit = trace(center, center + offset, [pawn, rock])
                    assert hit is not None and hit.to_tuple()[9] == bedrock, (str(center), str(offset), str(hit))
                    report["shell_checks"] += 1
                assert trace(first + unreal.Vector(0, 0, 120), second + unreal.Vector(0, 0, 120), [pawn, rock]) is None, "Bedrock blocks planned centerline"
            assert not bedrock.get_editor_property("hidden")
            report["bedrock_visible_in_game"] = True
        if now - state["started"] > 300 or now - state["progress_time"] > 20:
            finish(False, "Character stopped progressing toward route waypoint")
            return
        position = pawn.get_actor_location()
        target, room_order = targets[state["target"]]
        horizontal = target - position
        horizontal.z = 0
        distance = horizontal.length()
        if distance < state["best"] - 5:
            state["best"] = distance
            state["progress_time"] = now
        if distance < 80 and abs(position.z - target.z - capsule.get_scaled_capsule_half_height()) < 100:
            if room_order is not None:
                report["rooms_reached"].append(room_order)
            state["target"] += 1
            state["best"] = 1e9
            state["progress_time"] = now
            if state["target"] == len(targets):
                report["successful_cuts"] = rock.successful_cuts
                report["no_runtime_regeneration"] = generator.build_revision == state["revision"]
                mining.reset_field()
                report["reset_restores_network"] = rock.triangle_count == initial_triangles
                report["reset_restores_player"] = (pawn.get_actor_location() - start).length() < 20
                assert report["reset_restores_network"] and report["reset_restores_player"]
                finish(True, "Actual CharacterMovement and TryMine traversed all route waypoints without teleport, flight or disabled collision")
            return
        direction = horizontal / max(distance, 0.01)
        pitch = math.degrees(math.atan2(target.z - position.z + capsule.get_scaled_capsule_half_height(), max(distance, 1)))
        aim = unreal.Rotator(pitch=max(-30, min(30, pitch)), yaw=math.degrees(math.atan2(direction.y, direction.x)))
        if now - state["progress_time"] > 0.4:
            obstacle = unreal.SystemLibrary.capsule_trace_single(world, position, position + direction * 110, capsule.get_scaled_capsule_radius(), capsule.get_scaled_capsule_half_height(), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if obstacle:
                hit = obstacle.to_tuple()
                report["last_obstacle"] = str(hit[9])
                if isinstance(hit[9], unreal.DiggableRock):
                    aim = unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), hit[5] - hit[7] * 8)
                elif target.z < position.z - capsule.get_scaled_capsule_half_height():
                    footing = trace(position, position - unreal.Vector(0, 0, 400), [pawn])
                    if footing is not None and isinstance(footing.to_tuple()[9], unreal.DiggableRock):
                        for yaw_offset in (180, 90, -90, 0):
                            candidate = unreal.Rotator(pitch=-65, yaw=aim.yaw + yaw_offset)
                            view_origin = camera.get_world_location()
                            floor_hit = trace(view_origin, view_origin + unreal.MathLibrary.get_forward_vector(candidate) * mining.reach, [pawn])
                            if floor_hit is not None and isinstance(floor_hit.to_tuple()[9], unreal.DiggableRock):
                                aim = candidate
                                break
        controller.set_control_rotation(aim)
        pawn.add_movement_input(direction, 1.0, True)
        if now >= state["next_cut"]:
            mining.try_mine()
            state["next_cut"] = now + 0.34
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("ROUTE_RUNTIME_ARMED")
