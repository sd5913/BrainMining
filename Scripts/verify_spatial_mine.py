import hashlib
import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)[0]
main = region.main_tunnel
shapes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialTunnel)
side = next(item for item in shapes if item.kind == unreal.MiningManualKind.SIDE_ROOM)
branch = next(item for item in shapes if item.kind == unreal.MiningManualKind.PASSAGE and item != main)
generator.set_editor_property("auto_rebuild", False)
report = {"passed": False, "checks": []}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def source():
    return {item.get_name(): [knot.export_text() for knot in item.knots] for item in shapes}


def snapshot():
    return (generator.build_revision, [item.get_path_name() for item in generator.generated_segments],
            [item.triangle_count for item in generator.generated_segments],
            [item.export_text() for item in generator.generated_map_regions],
            generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count())


def reject(label):
    previous = snapshot()
    check(not generator.rebuild_tunnels(), label + ": rejected")
    check(previous == snapshot(), label + ": previous terrain, collision owners and map retained")


def trace(start, end):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*end), unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[9] if hit else None


try:
    authored = source()
    check(generator.rebuild_tunnels(), generator.last_build_status)
    check(source() == authored, "Build does not rewrite authored XYZ knots, profiles, handles or roll")
    check(len(shapes) == 5 and generator.space_count == 4, "All spatial source types survive save/reload")
    check(generator.segment_triangle_budget == 30000, "Triangle budget unchanged")
    check(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), "Bedrock mesh closed")
    for segment in generator.generated_segments:
        check(segment.max_triangles == 30000 and len(segment.chunks) <= 128, "Existing chunk and triangle caps retained")
        check(all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks), "All generated 3D rock chunks closed")
    check(trace((450, 0, 500), (900, 0, 500)) is None, "Start room wall portal has no collision seam")
    check(trace((6500, 0, -300), (6900, 0, -300)) is None, "End room wall portal has no collision seam")
    check(trace((2800, 850, 250), (2800, 850, 1200)) is not None, "Curved tunnel roof has solid collision")
    check(trace((3830, -550, 0), (4100, -550, 0)) is not None, "Rock plug is solid before mining")
    saved_knots = [knot.copy() for knot in main.knots]
    for horizontal, vertical in [(-29.62963, -14.814815), (-20, 0), (18, 12)]:
        edited = [knot.copy() for knot in saved_knots]
        profile = [point.copy() for point in edited[1].profile]
        profile[3].set_editor_property("position", profile[3].position + unreal.Vector2D(horizontal, vertical))
        edited[1].set_editor_property("profile", profile)
        main.set_editor_property("knots", edited)
        check(generator.rebuild_tunnels(), "Small section drag with rock plug rebuilds: " + generator.last_build_status)
        check(trace((3830, -550, 0), (4100, -550, 0)) is not None, "Edited section retains solid rock plug")
        check(all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for segment in generator.generated_segments for chunk in segment.chunks), "Edited section keeps chunks closed")
    main.set_editor_property("knots", saved_knots)
    check(generator.rebuild_tunnels(), "Section drag regression restores baseline: " + generator.last_build_status)
    invalid = [knot.copy() for knot in main.knots]
    invalid[2].set_editor_property("section_scale", unreal.Vector2D(0, 1))
    main.set_editor_property("knots", invalid)
    reject("Zero section width")
    main.set_editor_property("knots", saved_knots)
    invalid = [knot.copy() for knot in main.knots]
    profile = [point.copy() for point in invalid[2].profile]
    profile[1].set_editor_property("position", profile[0].position)
    invalid[2].set_editor_property("profile", profile)
    main.set_editor_property("knots", invalid)
    reject("Duplicate profile control points")
    main.set_editor_property("knots", saved_knots)
    original_size = region.rock_size.copy()
    region.set_editor_property("rock_size", unreal.Vector(1000, 1000, 1000))
    reject("Source outside region")
    region.set_editor_property("rock_size", original_size)
    check(generator.rebuild_tunnels(), "Valid geometry recovers after invalid drafts: " + generator.last_build_status)
    original_side = side.get_actor_location()
    side.set_actor_location(original_side + unreal.Vector(30, -30, 40), False, True)
    check(generator.rebuild_tunnels(), "Moving rotated side room rebuilds bound branch: " + generator.last_build_status)
    check(source() == authored, "Room-following portals do not replace authored branch nodes")
    side.set_actor_location(original_side, False, True)
    room = region.from_room
    size = room.room_size.copy()
    room.set_editor_property("room_size", unreal.Vector(size.x, size.y, size.z + 80))
    check(generator.rebuild_tunnels(), "Main room height change follows portal")
    check(source() == authored, "Main room edits preserve source knots")
    room.set_editor_property("room_size", size)
    check(generator.rebuild_tunnels(), "Restored demo rebuilds")
    report.update({"passed": True, "status": generator.last_build_status})
except Exception:
    report["error"] = traceback.format_exc()
finally:
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "spatial_geometry.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_GEOMETRY " + json.dumps(report))
