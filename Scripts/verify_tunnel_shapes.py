import gc
import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "timings": []}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "tunnel_shapes.json"
spawned = []
generator.set_editor_property("auto_rebuild", False)


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def snapshot():
    result = [(region.name, region.group_index, [(point.x, point.y, point.z) for point in region.vertices]) for region in generator.generated_map_regions]
    gc.collect()
    return result


def rebuild(label):
    gc.collect()
    started = time.perf_counter()
    check(generator.rebuild_tunnels(), label + ": " + generator.last_build_status)
    report["timings"].append({"label": label, "ms": (time.perf_counter() - started) * 1000})
    check(query.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), label + ": closed bedrock")
    check(generator.segment_triangle_budget == 30000, label + ": budget stays 30000")
    for segment in generator.generated_segments:
        check(segment.largest_chunk_triangles <= 30000, label + ": chunks respect budget")
        for chunk in segment.chunks:
            check(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()), label + ": closed mining chunk")


def rejected(label):
    previous = snapshot()
    revision = generator.build_revision
    segments = list(generator.generated_segments)
    triangles = generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count()
    check(not generator.rebuild_tunnels(), label + ": invalid edit rejected")
    check(generator.build_revision == revision and list(generator.generated_segments) == segments, label + ": revision and mining actors unchanged")
    check(snapshot() == previous, label + ": old minimap retained")
    check(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count() == triangles, label + ": bedrock unchanged")


def trace(first, second, ignored):
    return unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*first), unreal.Vector(*second), unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored, unreal.DrawDebugTrace.NONE, True)


try:
    rebuild("saved layout without shape controls")
    initial = snapshot()
    for space in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace):
        space.set_editor_property("enabled", False)
    for index, room in enumerate(rooms):
        room.set_actor_location(unreal.Vector(index * 9000, 0, 0), False, True)
        room.set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
        if index > 1:
            room.set_editor_property("layout_id", "ShapeTestExcluded")
    rebuild("two-room fixture")
    route = generator.routes[0]
    baseline = snapshot()
    neutral = route.create_shape_section()
    check(neutral is not None, "Add Shape Section creates selectable editor actor")
    spawned.append(neutral)
    check(neutral.get_actor_location().distance(unreal.Vector(4500, 0, 600)) < 0.1, "New section starts on existing tunnel")
    rebuild("neutral midpoint")
    controls = [neutral, route.create_shape_section(), route.create_shape_section()]
    check(all(controls), "Repeated add subdivides largest remaining gap")
    spawned.extend(controls[1:])
    coordinates = [(2500, 600, 650), (4500, -800, 900), (6500, 400, 650)]
    for section, point, width, height in zip(controls, coordinates, [1000, 1800, 1100], [1200, 1600, 1200]):
        section.set_actor_location(unreal.Vector(*point), False, True)
        section.set_editor_property("width", width)
        section.set_editor_property("height", height)
    rebuild("three-frame variable-width bend")
    check(generator.tunnel_count == 1 and len(generator.generated_segments) == 1, "Shape frames do not create progression groups")
    check(len(generator.generated_map_regions) == 6, "Minimap stores four convex sections, not one convex hull")
    for first, second in zip([(501, 0, 600)] + coordinates, coordinates + [(8499, 0, 600)]):
        check(trace(first, second, list(generator.generated_segments)) is None, "No bedrock wall across a shaped-section seam")
    check(trace((4500, -800, 900), (4500, -2300, 900), list(generator.generated_segments)) is not None, "Bedrock follows expanded cross-section")
    check(trace((4500, -800, 2300), (4500, -800, 900), [generator.generated_bedrock]) is not None, "Edited passage remains filled with diggable rock")
    rooms[1].set_actor_location(unreal.Vector(10000, 200, -300), False, True)
    rooms[1].set_editor_property("room_size", unreal.Vector(1200, 1800, 1400))
    rebuild("move and resize endpoint room")
    check(neutral.get_actor_location().distance(unreal.Vector(*coordinates[0])) < 0.1, "Room edits preserve authored world-space controls")
    rooms[1].set_actor_location(unreal.Vector(9000, 0, 0), False, True)
    rooms[1].set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
    rebuild("restore endpoint")
    generator.set_editor_property("direct_room_faces", False)
    rejected("legacy routing cannot silently discard shape controls")
    generator.set_editor_property("direct_room_faces", True)
    neutral.set_actor_location(unreal.Vector(4500, 0, 600), False, True)
    rejected("coincident control")
    neutral.set_actor_location(unreal.Vector(*coordinates[0]), False, True)
    neutral.set_editor_property("width", 1)
    rejected("invalid width")
    neutral.set_editor_property("width", 1000)
    neutral.set_actor_rotation(unreal.Rotator(0, 20, 0), False)
    rejected("rotated section")
    neutral.set_actor_rotation(unreal.Rotator(), False)
    neutral.set_actor_location(unreal.Vector(9500, 0, 600), False, True)
    rejected("control beyond room face")
    neutral.set_actor_location(unreal.Vector(*coordinates[0]), False, True)
    neutral.set_actor_scale3d(unreal.Vector(1, 1.2, 1.1))
    rebuild("positive nonuniform frame scale")
    neutral.set_actor_scale3d(unreal.Vector(1, 1, 1))
    cave = actors.spawn_actor_from_class(unreal.MiningLayoutSpace, unreal.Vector(4500, -4000, 100))
    spawned.append(cave)
    cave.set_editor_property("from_room", rooms[0])
    cave.set_editor_property("space_size", unreal.Vector(1400, 1200, 1200))
    cave.set_editor_property("connection", unreal.MiningSpaceConnection.STRAIGHT_BRANCH)
    cave.set_editor_property("entrance", unreal.MiningSpaceEntrance.OPEN)
    rebuild("branch on shaped tunnel")
    check(cave.resolved_branch_anchor.y < -500, "Branch attaches to actual bend instead of old straight centerline")
    cave.set_editor_property("entrance", unreal.MiningSpaceEntrance.HIDDEN_WALL)
    rebuild("hidden cave on shaped tunnel")
    check(cave.resolved_wall_size.x > 0, "Hidden entrance retained")
    cave.set_editor_property("enabled", False)
    rooms[2].set_editor_property("layout_id", rooms[0].layout_id)
    rooms[2].set_actor_location(unreal.Vector(4500, 6000, 0), False, True)
    controls[2].set_actor_location(unreal.Vector(6500, 4500, 650), False, True)
    rejected("shaped tunnel crosses another tunnel group")
    controls[2].set_actor_location(unreal.Vector(*coordinates[2]), False, True)
    rooms[2].set_editor_property("layout_id", rooms[0].layout_id)
    rooms[2].set_actor_location(unreal.Vector(4500, -800, 0), False, True)
    rejected("shaped tunnel crosses another main room")
    rooms[2].set_editor_property("layout_id", "ShapeTestExcluded")
    route.restore_straight_tunnel()
    check(all(not section.enabled for section in controls), "Restore Straight preserves controls but disables them")
    rebuild("restored straight connection")
    check(snapshot() == baseline, "No controls reproduces original geometry snapshot")
    for section in controls:
        section.set_editor_property("enabled", True)
    rebuild("re-enable preserved edits")
    for section in controls:
        actors.destroy_actor(section)
    rebuild("delete controls")
    check(snapshot() == baseline, "Deletion restores direct full-face connection")
    generator.set_editor_property("require_descending", False)
    for direction in [(0, 1, 0), (0, -1, 0), (0, 0, -1), (0, 0, 1), (-1, 0, 0)]:
        rooms[1].set_actor_location(unreal.Vector(*[value * 9000 for value in direction]), False, True)
        rebuild("alternate face fixture " + str(direction))
        section = route.create_shape_section()
        spawned.append(section)
        position = section.get_actor_location()
        position.x += 700 if direction[0] == 0 else 0
        position.y += 0 if direction[0] == 0 else 700
        section.set_actor_location(position, False, True)
        section.set_editor_property("width", 1800)
        rebuild("alternate face bent geometry " + str(direction))
        actors.destroy_actor(section)
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    gc.collect()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("TUNNEL_SHAPES " + json.dumps(report))
assert report["passed"], report.get("error")
