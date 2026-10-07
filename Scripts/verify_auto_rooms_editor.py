import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = editor.get_editor_world()
assert "Lvl_MiningDescent_AutoRooms" in world.get_path_name()
assert editor.get_game_world() is None
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
originals = [(room.get_actor_transform(), unreal.Vector(room.room_size.x, room.room_size.y, room.room_size.z)) for room in rooms]
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
report = {"passed": False, "automatic_edits": []}
state = {"phase": 0, "revision": generator.build_revision, "time": time.monotonic(), "extra": None}
generator.set_editor_property("auto_rebuild", True)
rooms[1].set_actor_location(rooms[1].get_actor_location() + unreal.Vector(120, -200, -60), False, True)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if state["extra"] is not None:
        actors.destroy_actor(state["extra"])
    for room, (transform, size) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
    generator.rebuild_tunnels()
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    report["passed"] = passed
    report["reason"] = reason
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "auto_rooms_editor.json"
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    lowest = min(room.get_actor_location().z for room in rooms)
    highest = max(room.get_actor_location().z + room.room_size.z * room.get_actor_scale3d().z for room in rooms)
    center_x = sum(room.get_actor_location().x for room in rooms) / len(rooms)
    editor.set_level_viewport_camera_info(unreal.Vector(center_x, max(6000, (highest - lowest) * 1.8), (lowest + highest) / 2), unreal.Rotator(pitch=0, yaw=-90))
    actors.set_selected_level_actors([rooms[1]])
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AUTO_ROOMS_EDITOR " + json.dumps(report))


def tick(delta):
    try:
        if time.monotonic() - state["time"] > 15:
            finish(False, "No automatic rebuild: " + generator.last_build_status)
            return
        if generator.build_revision <= state["revision"]:
            return
        report["automatic_edits"].append({"phase": state["phase"], "revision": generator.build_revision, "rooms": generator.room_count, "ms": generator.last_build_milliseconds})
        assert generator.last_build_status.startswith("Ready:"), generator.last_build_status
        assert len(generator.generated_segments) == generator.tunnel_count
        assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock)) == generator.tunnel_count
        state["phase"] += 1
        state["revision"] = generator.build_revision
        state["time"] = time.monotonic()
        if state["phase"] == 1:
            rooms[2].set_editor_property("room_size", unreal.Vector(1250, 1450, 600))
        elif state["phase"] == 2:
            rooms[3].set_actor_scale3d(unreal.Vector(1.15, 1.1, 1.05))
        elif state["phase"] == 3:
            extra = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, rooms[-1].get_actor_location() + unreal.Vector(3000, 0, -400))
            state["extra"] = extra
            extra.set_editor_property("connection_order", 4)
        elif state["phase"] == 4:
            assert generator.room_count == 5 and generator.tunnel_count == 4
            actors.destroy_actor(state["extra"])
            state["extra"] = None
        else:
            assert generator.room_count == 4 and generator.tunnel_count == 3
            finish(True, "Position, dimensions, nonuniform scale, adding and removing rooms all rebuilt without calling RebuildTunnels")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("AUTO_ROOMS_EDITOR_ARMED")
