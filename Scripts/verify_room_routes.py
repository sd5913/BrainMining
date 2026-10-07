import json
import math
import traceback
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level(MAP_PATH)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
report = {"passed": False, "checks": []}
query = unreal.GeometryScript_MeshQueries


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def snapshot():
    shell = generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()
    fill = generator.generated_fill.rock_mesh.get_dynamic_mesh()
    return {"shell_closed": query.get_is_closed_mesh(shell), "fill_closed": query.get_is_closed_mesh(fill), "shell_triangles": shell.get_triangle_count(), "fill_triangles": fill.get_triangle_count(), "revision": generator.build_revision, "status": generator.last_build_status}


def rebuild():
    check(generator.rebuild_tunnels(), generator.last_build_status)
    check(generator.layout_applied, "Layout applied flag matches successful generation")
    state = snapshot()
    check(state["shell_closed"] and state["fill_closed"], "Both generated meshes are closed")
    return state


def trace(first, second):
    return unreal.SystemLibrary.line_trace_single(world, first, second, unreal.TraceTypeQuery.ECC_VISIBILITY, False, [generator.generated_fill], unreal.DrawDebugTrace.NONE, True)


def points(route):
    return [route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(route.path.get_number_of_spline_points())]


try:
    loaded = snapshot()
    report["loaded"] = loaded
    generator.generated_fill.reset_rock()
    check(generator.generated_fill.triangle_count == loaded["fill_triangles"], "Saved custom mesh survives reload and reset")
    baseline = rebuild()
    check(baseline["fill_triangles"] == loaded["fill_triangles"], "Reloaded and regenerated topology matches")
    report["routes"] = []
    for route in generator.routes:
        route_points = points(route)
        check(route.maximum_slope <= generator.max_slope_degrees + 0.01, "Route respects maximum slope")
        report["routes"].append({"slope": route.maximum_slope, "length": route.route_length, "points": [[point.x, point.y, point.z] for point in route_points]})
        for entrance, landing in ((route_points[0], route_points[1]), (route_points[-1], route_points[-2])):
            direction = landing - entrance
            direction = direction / direction.length()
            check(abs(entrance.z - landing.z) < 0.01, "Doorway landing is level")
            hit = trace(entrance - direction * 60 + unreal.Vector(0, 0, 100), landing + unreal.Vector(0, 0, 100))
            check(hit is None, "Bedrock does not block doorway")
    for room in rooms:
        floor = room.get_actor_location()
        height = room.room_size.z * abs(room.get_actor_scale3d().z)
        check(trace(floor + unreal.Vector(0, 0, 100), floor + unreal.Vector(0, 0, height - 20)) is None, "Room interior has no stray bedrock")
        hit = trace(floor + unreal.Vector(0, 0, 100), floor - unreal.Vector(0, 0, 50))
        check(hit is not None and abs(hit.to_tuple()[5].z - floor.z) < 1, "Room floor preserved")
    bounds_origin, bounds_extent = generator.generated_fill.get_actor_bounds(False)
    lowest = min(room.get_actor_location().z for room in rooms)
    highest = max(room.get_actor_location().z + room.room_size.z * abs(room.get_actor_scale3d().z) for room in rooms)
    check(bounds_origin.z - bounds_extent.z >= lowest - 1 and bounds_origin.z + bounds_extent.z <= highest + 1, "No fill protrudes below lowest floor or above highest ceiling")
    original_size = rooms[1].room_size
    original_position = rooms[1].get_actor_location()
    rooms[1].set_editor_property("room_size", original_size + unreal.Vector(0, 0, 200))
    rebuild()
    check((rooms[1].get_actor_location() - original_position).length() < 0.01, "Changing room height keeps floor anchor fixed")
    rooms[1].set_editor_property("room_size", original_size)
    rebuild()
    route = generator.routes[0]
    route.lock_route_for_editing()
    manual_points = points(route)
    if len(manual_points) > 4:
        route.path.set_location_at_spline_point(2, manual_points[2] + unreal.Vector(3, 0, 0), unreal.SplineCoordinateSpace.WORLD, True)
    manual_points = points(route)
    rebuild()
    check(all((old - new).length() < 0.1 for old, new in zip(manual_points, points(route))), "Locked manual control points survive rebuild")
    route.restore_automatic_route()
    rebuild()
    last_good = snapshot()
    rooms[1].set_editor_property("connection_order", rooms[0].connection_order)
    check(not generator.rebuild_tunnels() and not generator.layout_applied, "Duplicate order rejects the pending layout")
    check(generator.build_revision == last_good["revision"] and generator.generated_fill.triangle_count == last_good["fill_triangles"], "Invalid input retains previous geometry")
    rooms[1].set_editor_property("connection_order", 1)
    rebuild()
    generator.set_editor_property("require_descending", True)
    check(not generator.rebuild_tunnels() and "descending" in generator.last_build_status, "Descending-only check rejects current uphill start without moving rooms")
    generator.set_editor_property("require_descending", False)
    rebuild()
    generator.set_editor_property("max_slope_degrees", 5)
    generator.set_editor_property("search_distance", 1000)
    check(not generator.rebuild_tunnels() and not generator.layout_applied, "Impossible bounded search clearly fails instead of creating a steep tube")
    generator.set_editor_property("max_slope_degrees", 30)
    generator.set_editor_property("search_distance", 12000)
    rebuild()
    old_revision = generator.build_revision
    old_triangles = generator.generated_fill.triangle_count
    generator.show_section()
    generator.show_game_geometry()
    generator.show_layout()
    check(generator.build_revision == old_revision and generator.generated_fill.triangle_count == old_triangles, "Preview switching never edits collision or mining geometry")
    extra = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(9000, 2400, -1600))
    extra.set_editor_property("connection_order", 4)
    rebuild()
    check(generator.room_count == 5 and len(generator.routes) == 4, "Fifth room creates one additional route")
    actors.destroy_actor(extra)
    rebuild()
    check(generator.room_count == 4 and len(generator.routes) == 3, "Removing a room removes its obsolete route")
    for attempt in range(2):
        rebuild()
    route_actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelRoute)
    check(len(route_actors) == 3, "Repeated rebuilds do not duplicate route actors")
    report["final"] = snapshot()
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "route_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("ROUTE_GEOMETRY " + json.dumps(report))
assert report["passed"], report.get("error")
