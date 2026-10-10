import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")
source_world = editor.get_editor_world()
region = unreal.GameplayStatics.get_all_actors_of_class(source_world, unreal.MiningSpatialRegion)[0]
main = region.main_tunnel
branch = next(item for item in unreal.GameplayStatics.get_all_actors_of_class(source_world, unreal.MiningSpatialTunnel) if item != main and item.kind == unreal.MiningManualKind.PASSAGE)
side = branch.end_portal.room


def evaluate(first, last, parameter):
    inverse = 1 - parameter
    return first.position * inverse ** 3 + (first.position + first.leave) * (3 * inverse * inverse * parameter) + (last.position + last.arrive) * (3 * inverse * parameter * parameter) + last.position * parameter ** 3


targets = []
for segment in range(len(main.knots) - 1):
    for step in range(1, 13):
        targets.append((evaluate(main.knots[segment], main.knots[segment + 1], step / 12), "main"))
    if segment == 1:
        branch_points = [evaluate(branch.knots[0], branch.knots[1], step / 10) for step in range(1, 11)]
        targets.extend((position, "branch") for position in branch_points)
        targets.append((side.get_actor_location(), "side_room"))
        targets.extend((position, "return") for position in reversed(branch_points))
        targets.append((main.knots[2].position.copy(), "rejoin"))
targets.append((unreal.Vector(7200, 0, -300), "finish"))
report = {"passed": False, "checks": [], "scope": "Real CharacterMovement, held ascent and TryMine; no teleport, flight or collision overrides"}
state = {"started": time.monotonic(), "phase": 0, "target": 0, "progress": time.monotonic(), "best": 1e9, "next_cut": 0, "cuts": 0}
performance = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = performance.get_editor_property("bThrottleCPUWhenNotForeground")
performance.set_editor_property("bThrottleCPUWhenNotForeground", False)


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def finish(passed):
    unreal.unregister_slate_post_tick_callback(callback)
    performance.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report.update({"passed": passed, "elapsed": time.monotonic() - state["started"], "target": state["target"], "cuts": state["cuts"]})
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "spatial_runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_RUNTIME " + json.dumps(report))
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()


def tick(delta):
    try:
        now = time.monotonic()
        assert now - state["started"] < 300, "Runtime timeout"
        if state["phase"] == 0:
            if now - state["started"] > 3:
                levels.editor_request_begin_play()
                state["phase"] = 1
            return
        world = editor.get_game_world()
        if world is None:
            return
        pawn = unreal.GameplayStatics.get_player_character(world, 0)
        if pawn is None:
            return
        generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
        rock = generator.generated_segments[0]
        movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
        mining = pawn.get_component_by_class(unreal.MiningComponent)
        camera = pawn.get_component_by_class(unreal.CameraComponent)
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        minimap = controller.get_hud().get_component_by_class(unreal.MiningDeveloperMapComponent)
        if state["phase"] == 1:
            check(generator.layout_applied and rock.max_triangles == 30000, "PIE uses valid geometry at original triangle budget")
            state.update({"phase": 2, "progress": now, "revision": generator.build_revision, "initial_triangles": rock.triangle_count})
        position = pawn.get_actor_location()
        check_state = generator.current_room_index
        if state["target"] >= len(targets) or check_state == 1:
            check(check_state == 1, "Reached next main room by traversing nonplanar route")
            check(state["cuts"] > 0, "Real mining input used through 3D rock plug")
            check(rock.segment_state != unreal.MiningSegmentState.ACTIVE, "Previous region is frozen/recycled after arrival")
            check(generator.build_revision == state["revision"], "Editor does not rebuild during PIE")
            check(rock.largest_chunk_triangles <= 30000, "Mining remains within original per-chunk budget")
            finish(True)
            return
        target, label = targets[state["target"]]
        offset = target - position
        remaining = (offset.x ** 2 + offset.y ** 2) ** 0.5 + abs(offset.z) * 0.3
        if remaining < state["best"] - 3:
            state["best"] = remaining
            state["progress"] = now
        if abs(offset.x) < 65 and abs(offset.y) < 65 and abs(offset.z) < 110:
            if label == "side_room":
                check(check_state == 0 and rock.segment_state == unreal.MiningSegmentState.ACTIVE, "Branch room does not trigger progression")
                check("支线房间" in minimap.location_label, "3D minimap locates rotated branch room")
            state["target"] += 1
            state["best"] = 1e9
            state["progress"] = now
            return
        assert now - state["progress"] < 35, "Stuck at %s toward %s (%d): %s" % (position, target, state["target"], mining.status)
        if offset.z > 45:
            pawn.do_jump_start()
        else:
            pawn.do_jump_end()
        direction = unreal.Vector(offset.x, offset.y, 0)
        distance = direction.length()
        if distance > 20:
            pawn.add_movement_input(direction / distance, min(1, distance / 160), True)
        else:
            movement.stop_movement_immediately()
        aim = position + offset / max(1, offset.length()) * min(mining.reach * 0.85, offset.length())
        controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), aim))
        if now >= state["next_cut"] and now - state["progress"] > 0.15:
            before = rock.triangle_count
            mining.try_mine()
            if rock.triangle_count != before:
                state["cuts"] += 1
            state["next_cut"] = now + 0.3
    except Exception:
        report["error"] = traceback.format_exc()
        finish(False)


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("SPATIAL_RUNTIME_ARMED")
