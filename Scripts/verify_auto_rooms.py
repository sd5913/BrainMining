import json
import traceback
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if MAP_PATH not in editor.get_editor_world().get_path_name():
    assert levels.load_level(MAP_PATH)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
originals = [(room.get_actor_transform(), unreal.Vector(room.room_size.x, room.room_size.y, room.room_size.z), room.connection_order) for room in rooms]
old_auto = generator.auto_rebuild
generator.set_editor_property("auto_rebuild", False)
report = {"passed": False, "map": MAP_PATH}
extra = None
query = unreal.GeometryScript_MeshQueries


def snapshot():
    shell = generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()
    fill = generator.generated_fill.rock_mesh.get_dynamic_mesh()
    return {
        "revision": generator.build_revision,
        "status": generator.last_build_status,
        "shell_closed": query.get_is_closed_mesh(shell),
        "fill_closed": query.get_is_closed_mesh(fill),
        "shell_triangles": shell.get_triangle_count(),
        "fill_triangles": fill.get_triangle_count(),
        "shell_volume": query.get_mesh_volume_area(shell)[1],
        "fill_volume": query.get_mesh_volume_area(fill)[1],
        "shell_components": query.get_num_connected_components(shell),
        "build_ms": generator.last_build_milliseconds,
    }


try:
    initial = snapshot()
    report["loaded_without_regeneration"] = initial
    assert initial["shell_closed"] and initial["fill_closed"]
    assert initial["fill_triangles"] > 12
    rock = generator.generated_fill
    rock.reset_rock()
    assert rock.triangle_count == initial["fill_triangles"], "Saved custom reset mesh was lost"
    report["saved_reset_mesh_preserved"] = True
    assert generator.rebuild_tunnels(), generator.last_build_status
    initial = snapshot()
    report["current_algorithm_baseline"] = initial
    assert not isinstance(generator.generated_bedrock, unreal.DiggableRock)
    generated_paths = [actor.get_path_name() for actor in (generator.generated_bedrock, rock, generator.generated_player_start)]
    rooms[1].set_actor_location(rooms[1].get_actor_location() + unreal.Vector(100, -280, -75), False, True)
    rooms[2].set_editor_property("room_size", unreal.Vector(1350, 1600, 650))
    rooms[3].set_actor_scale3d(unreal.Vector(1.15, 1.2, 1.1))
    assert generator.rebuild_tunnels(), generator.last_build_status
    changed = snapshot()
    report["move_resize_nonuniform_scale"] = changed
    assert changed["shell_closed"] and changed["fill_closed"]
    assert abs(changed["fill_volume"] - initial["fill_volume"]) > 100
    assert [actor.get_path_name() for actor in (generator.generated_bedrock, rock, generator.generated_player_start)] == generated_paths
    extra = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(7700, 550, -700))
    extra.set_editor_property("connection_order", 4)
    assert generator.rebuild_tunnels(), generator.last_build_status
    report["fifth_room"] = snapshot()
    assert generator.room_count == 5 and generator.tunnel_count == 4
    actors.destroy_actor(extra)
    extra = None
    assert generator.rebuild_tunnels(), generator.last_build_status
    changed = snapshot()
    revision = generator.build_revision
    rooms[1].set_editor_property("connection_order", rooms[0].connection_order)
    assert not generator.rebuild_tunnels()
    assert generator.build_revision == revision and rock.triangle_count == changed["fill_triangles"]
    report["duplicate_order_keeps_previous_geometry"] = True
    rooms[1].set_editor_property("connection_order", originals[1][2])
    rooms[1].set_actor_rotation(unreal.Rotator(yaw=20), False)
    assert not generator.rebuild_tunnels()
    assert generator.build_revision == revision
    report["unsupported_rotation_keeps_previous_geometry"] = True
    for room, (transform, size, order) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
        room.set_editor_property("connection_order", order)
    assert generator.rebuild_tunnels(), generator.last_build_status
    for attempt in range(3):
        assert generator.rebuild_tunnels()
    assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock)) == 1
    assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DynamicMeshActor)) == 1
    report["rebuild_reuses_actors"] = True
    report["restored"] = snapshot()
    report["restored_inputs"] = [{"location": str(room.get_actor_location()), "size": str(room.room_size), "scale": str(room.get_actor_scale3d())} for room in rooms]
    assert abs(report["restored"]["fill_volume"] - initial["fill_volume"]) < 1
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
    raise
finally:
    if extra is not None:
        actors.destroy_actor(extra)
    for room, (transform, size, order) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
        room.set_editor_property("connection_order", order)
    generator.rebuild_tunnels()
    generator.set_editor_property("auto_rebuild", old_auto)
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "auto_rooms_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AUTO_ROOMS_GEOMETRY " + json.dumps(report))
