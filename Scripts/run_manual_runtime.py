import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo")
targets = [(800, 450), (1500, 450), (2250, 550), (2800, 250), (3450, -350), (3950, -130), (4200, -600), (4600, -1000), (4600, -1450), (4600, -1000), (4200, -600), (3950, -130), (4750, 80), (5500, -150), (6150, -300), (6850, -300), (7200, -500)]
report = {"passed": False, "checks": [], "scope": "Real CharacterMovement, held ascent and TryMine; no teleport/flight/collision changes during traversal", "labels": []}
state = {"started": time.monotonic(), "phase": 0, "target": 0, "progress": time.monotonic(), "best": 1e9, "next_cut": 0}
performance = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = performance.get_editor_property("bThrottleCPUWhenNotForeground")
performance.set_editor_property("bThrottleCPUWhenNotForeground", False)


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    performance.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"], "target": state["target"]})
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "manual_runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MANUAL_RUNTIME " + json.dumps(report))
    if editor.get_game_world() is not None:
        levels.editor_request_end_play()
    if "ManualAutoQuit" in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        now = time.monotonic()
        assert now - state["started"] < 300, "Runtime test timeout"
        if state["phase"] == 0:
            if now - state["started"] < 3:
                return
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
        segment = generator.generated_segments[0]
        movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
        mining = pawn.get_component_by_class(unreal.MiningComponent)
        camera = pawn.get_component_by_class(unreal.CameraComponent)
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        minimap = controller.get_hud().get_component_by_class(unreal.MiningDeveloperMapComponent)
        if state["phase"] == 1:
            state.update({"revision": generator.build_revision, "initial": segment.triangle_count, "phase": 2, "progress": now})
            authoring = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualShape)
            check(all(not actor.is_actor_tick_enabled() and actor.get_component_by_class(unreal.BoxComponent).get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for actor in authoring), "PIE authoring actors have no tick or collision; play uses baked geometry")
            check(minimap.view == unreal.MiningMapView.SIDE_XZ, "Manual minimap defaults to authored cross-section")
        check_no_rebuild = generator.build_revision == state["revision"]
        assert check_no_rebuild, "Runtime must not regenerate authored geometry"
        if minimap.location_label and minimap.location_label not in report["labels"]:
            report["labels"].append(minimap.location_label)
        if state["phase"] == 3:
            if now - state["reset_time"] < 1:
                return
            check(generator.current_room_index == 0 and segment.triangle_count == state["initial"], "Global reset restores original authored geometry and progression")
            check(segment.max_triangles == 30000 and len(segment.chunks) <= 128, "Runtime preserves 30000/128 budgets")
            finish(True, "Main route, side room detour, real digging, minimap, freeze and reset passed")
            return
        if state["target"] >= len(targets):
            pawn.do_jump_end()
            movement.stop_movement_immediately()
            if generator.current_room_index != 1:
                return
            check(segment.successful_cuts > 0, "Normal mining cuts through the authored block")
            check(segment.segment_state == unreal.MiningSegmentState.FROZEN, "Only main-room arrival freezes the completed region")
            check(not segment.mine_sphere(unreal.Vector(3200, 0, -100), 150), "Frozen region rejects more digging")
            report["cuts"] = segment.successful_cuts
            report["largest_chunk"] = segment.largest_chunk_triangles
            check(check_no_rebuild, "No runtime rebuild during traversal")
            mining.reset_field()
            state["phase"] = 3
            state["reset_time"] = now
            return
        position = pawn.get_actor_location()
        horizontal, vertical = targets[state["target"]]
        target = unreal.Vector(horizontal, 0, vertical)
        offset = target - position
        remaining = (offset.x * offset.x + offset.y * offset.y) ** 0.5 + abs(offset.z) * 0.35
        if remaining < state["best"] - 4:
            state["best"] = remaining
            state["progress"] = now
        if abs(offset.x) < 100 and abs(offset.y) < 80 and abs(offset.z) < 140:
            if state["target"] == 8:
                check(generator.current_room_index == 0 and segment.segment_state == unreal.MiningSegmentState.ACTIVE, "Entering branch room does not advance or freeze main progression")
                check("支线" in minimap.location_label, "Developer map locates the branch room")
            state["target"] += 1
            state["best"] = 1e9
            state["progress"] = now
            return
        if now - state["progress"] > 30:
            raise AssertionError("No movement progress at %s toward %s; %s" % (position, target, mining.status))
        if offset.z > 50:
            pawn.do_jump_start()
        else:
            pawn.do_jump_end()
        direction = unreal.Vector(offset.x, offset.y, 0)
        length = direction.length()
        if length > 20:
            pawn.add_movement_input(direction / length, min(1, length / 180), True)
        else:
            movement.stop_movement_immediately()
        aim = position + offset / max(offset.length(), 1) * min(mining.reach * 0.8, offset.length())
        controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), aim))
        if now >= state["next_cut"] and now - state["progress"] > 0.15:
            mining.try_mine()
            state["next_cut"] = now + 0.30
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("MANUAL_RUNTIME_ARMED")
