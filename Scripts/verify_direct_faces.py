import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
originals = [(room.get_actor_transform(), room.room_size, room.layout_id) for room in rooms]
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "cases": []}


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def rebuild():
    check(generator.rebuild_tunnels(), generator.last_build_status)
    check(query.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), "Bedrock is closed")
    check(all(query.get_is_closed_mesh(segment.get_combined_mesh()) for segment in generator.generated_segments), "All segment fills are closed")


def trace(first, second, ignored):
    return unreal.SystemLibrary.line_trace_single(world, first, second, unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignored, unreal.DrawDebugTrace.NONE, True)


def face(room, wall, across=0.5, up=0.5):
    origin = room.get_actor_location()
    size = room.room_size * room.get_actor_scale3d()
    center = [origin.x, origin.y, origin.z + size.z / 2]
    extents = [size.x / 2, size.y / 2, size.z / 2]
    axis = wall // 2
    side = 1 if axis == 0 else 0
    vertical = 1 if axis == 2 else 2
    center[axis] += extents[axis] * (1 if wall % 2 == 0 else -1)
    center[side] += (across * 2 - 1) * extents[side]
    center[vertical] += (up * 2 - 1) * extents[vertical]
    return unreal.Vector(*center)


def validate_links(label):
    for route in generator.routes:
        check(route.path.get_number_of_spline_points() == 2, label + ": exactly two endpoints, no detour")
        check(route.direct_connection, label + ": direct metadata")
        first_wall = route.get_editor_property("ResolvedStartWall")
        second_wall = route.get_editor_property("ResolvedEndWall")
        check(first_wall ^ 1 == second_wall, label + ": opposing faces")
        for across, up in ((0.02, 0.02), (0.98, 0.02), (0.98, 0.98), (0.02, 0.98), (0.5, 0.5)):
            first = face(route.from_room, first_wall, across, up)
            second = face(route.to_room, second_wall, across, up)
            direction = (second - first) / (second - first).length()
            check(trace(first - direction * 10, second + direction * 10, list(generator.generated_segments)) is None, label + ": full-face interior clear of bedrock")
            check(trace(first - direction * 10, first + direction * 20, [generator.generated_bedrock]) is not None, label + ": diggable fill reaches entrance edge")
        first = face(route.from_room, first_wall, 1, 0.5)
        second = face(route.to_room, second_wall, 1, 0.5)
        midpoint = (first + second) * 0.5
        tangent = second - first
        vertical = face(route.from_room, first_wall, 1, 1) - first
        normal = tangent.cross(vertical).normal()
        center = (face(route.from_room, first_wall) + face(route.to_room, second_wall)) * 0.5
        if normal.dot(midpoint - center) < 0:
            normal *= -1
        inner = trace(midpoint - normal * 20, midpoint + normal * 20, list(generator.generated_segments))
        outer = trace(midpoint + normal * (generator.bedrock_thickness + 40), midpoint + normal * 20, list(generator.generated_segments))
        check(inner is not None and outer is not None, label + ": shell has inner and outer surface")
        thickness = (inner.to_tuple()[5] - outer.to_tuple()[5]).length()
        check(abs(thickness - generator.bedrock_thickness) < 1, label + ": bedrock thickness preserved normal to sloped surface")
        report["cases"].append({"label": label, "from": route.from_room.connection_order, "slope": route.maximum_slope, "wall": first_wall, "thickness": thickness})


try:
    check(generator.direct_room_faces, "Saved map enables full-face mode")
    loaded_triangles = generator.generated_fill.triangle_count
    generator.generated_fill.reset_rock()
    check(generator.generated_fill.triangle_count == loaded_triangles, "Reload/reset preserves saved direct mesh")
    rebuild()
    validate_links("current user layout")
    second_position = rooms[1].get_actor_location()
    rooms[1].set_actor_location(second_position + unreal.Vector(120, -200, -60), False, True)
    rebuild()
    validate_links("moved user room")
    rooms[1].set_actor_location(second_position, False, True)
    rebuild()
    revision = generator.build_revision
    generator.show_section()
    generator.show_game_geometry()
    generator.show_layout()
    check(generator.build_revision == revision, "Editor preview modes do not rebuild mining geometry")
    for room in rooms[2:]:
        room.set_editor_property("layout_id", "DirectTestExcluded")
    first, second = rooms[:2]
    first.set_actor_scale3d(unreal.Vector(1, 1, 1))
    second.set_actor_scale3d(unreal.Vector(1, 1, 1))
    first.set_actor_location(unreal.Vector(0, 0, 0), False, True)
    first.set_editor_property("room_size", unreal.Vector(1000, 1200, 700))
    cases = (
        ("equal X faces", (3000, 0, -300), (1000, 1200, 700), 0),
        ("tapered X faces", (3000, 400, -700), (800, 800, 400), 0),
        ("negative X faces", (-3000, -200, -400), (1200, 1400, 900), 1),
        ("Y faces", (400, 3500, -500), (1400, 1100, 600), 2),
        ("steep without rerouting", (2100, 200, -3600), (1000, 1200, 700), 0),
        ("vertical shaft", (0, 0, -3000), (800, 900, 700), 5),
    )
    for label, position, size, expected_wall in cases:
        second.set_actor_location(unreal.Vector(*position), False, True)
        second.set_editor_property("room_size", unreal.Vector(*size))
        rebuild()
        check(generator.routes[0].get_editor_property("ResolvedStartWall") == expected_wall, label + ": intended face selected")
        validate_links(label)
    route = generator.routes[0]
    revision = generator.build_revision
    triangle_count = generator.generated_fill.triangle_count
    route.set_editor_property("start_wall", unreal.MiningDoorWall.POSITIVE_X)
    route.set_editor_property("end_wall", unreal.MiningDoorWall.POSITIVE_X)
    check(not generator.rebuild_tunnels(), "Invalid face pairing rejected")
    check(generator.build_revision == revision and generator.generated_fill.triangle_count == triangle_count, "Invalid edit preserves last working geometry")
    route.set_editor_property("start_wall", unreal.MiningDoorWall.AUTO)
    route.set_editor_property("end_wall", unreal.MiningDoorWall.AUTO)
    rebuild()
    check(len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelRoute)) == 1, "No stale or duplicate links after room removal")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    for room, (transform, size, layout_id) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
        room.set_editor_property("layout_id", layout_id)
    for route in generator.routes:
        route.set_editor_property("start_wall", unreal.MiningDoorWall.AUTO)
        route.set_editor_property("end_wall", unreal.MiningDoorWall.AUTO)
    report["restored"] = generator.rebuild_tunnels()
    generator.set_editor_property("auto_rebuild", True)
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "direct_faces_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DIRECT_FACES_GEOMETRY " + json.dumps(report))
assert report["passed"], report.get("error")
