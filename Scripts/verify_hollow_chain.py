import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
regions = list(generator.hollow_regions)
chambers = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowChamber), key=lambda chamber: chamber.get_actor_label())
generator.set_editor_property("auto_rebuild", False)
report = {"passed": False, "checks": []}


def check(condition, name):
    assert condition, name
    report["checks"].append(name)


def snapshot():
    return {
        "revision": generator.build_revision,
        "segments": [segment.get_path_name() for segment in generator.generated_segments],
        "triangles": [segment.triangle_count for segment in generator.generated_segments],
        "bedrock": generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count(),
        "map": [item.export_text() for item in generator.generated_map_regions],
        "paths": [[region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD).to_tuple() for index in range(region.path.get_number_of_spline_points())] for region in regions],
    }


def reject(name):
    before = snapshot()
    check(not generator.rebuild_tunnels(), name + ": rejected")
    after = snapshot()
    if before != after:
        report["snapshot_changes"] = {key: {"before": before[key], "after": after[key]} for key in before if before[key] != after[key]}
    check(before == after, name + ": last successful geometry, groups and map retained")


def trace(start, end, ignored=None):
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored or [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[9] if hit else None


try:
    check(len(regions) == 3 and generator.room_count == 4 and generator.space_count == 3, "Saved chain references and generated counts survive reload")
    check(generator.rebuild_tunnels(), generator.last_build_status)
    check(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), "Shared bedrock is closed")
    for index, region in enumerate(regions):
        segment = generator.generated_segments[index]
        check(segment.segment_index == index and segment.max_triangles == 30000 and len(segment.chunks) <= 128, "Region %d retains independent original budgets" % index)
        check(all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks), "Region %d chunks closed" % index)
        guides = list(segment.guide_material_volumes)
        check(bool(guides) and all(region.from_room.get_actor_location().x < guide.start.x < region.to_room.get_actor_location().x for guide in guides), "Region %d owns only its own plug masks" % index)
        plug = guides[0]
        direction = (plug.end - plug.start) / (plug.end - plug.start).length()
        check(trace(plug.start - direction * 50, plug.end + direction * 50) == segment, "Region %d plug has mineable collision" % index)
        start = region.path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD)
        end = region.path.get_location_at_spline_point(region.path.get_number_of_spline_points() - 1, unreal.SplineCoordinateSpace.WORLD)
        check(trace(start - unreal.Vector(100, 0, 0), start + unreal.Vector(100, 0, -15)) is None, "Region %d entry portal open" % index)
        check(trace(end - unreal.Vector(100, 0, -15), end + unreal.Vector(100, 0, 0)) is None, "Region %d exit portal open" % index)
        anchor = region.path.get_location_at_distance_along_spline(region.path.get_spline_length() * 0.40, unreal.SplineCoordinateSpace.WORLD)
        ignored = list(generator.generated_segments)
        check(trace(anchor, anchor + unreal.Vector(0, 1800, 0), ignored) is None, "Region %d has no internal bedrock sleeve" % index)
        check(trace(anchor + unreal.Vector(0, 2000, 0), anchor + unreal.Vector(0, 2800, 0), ignored) == generator.generated_bedrock, "Region %d outer bedrock collides" % index)
        kinds = {item.kind for item in generator.generated_map_regions if item.group_index == index}
        check({unreal.MiningMapRegionKind.ROCK_REGION, unreal.MiningMapRegionKind.TUNNEL, unreal.MiningMapRegionKind.GUIDE_PLUG, unreal.MiningMapRegionKind.BRANCH, unreal.MiningMapRegionKind.SPACE}.issubset(kinds), "Region %d minimap owns rock, path, plugs and cave" % index)
    generator.set_editor_property("hollow_regions", [regions[2], regions[0], regions[1]])
    check(generator.rebuild_tunnels(), "References resolve by room pair rather than list order")
    check(generator.generated_segments[1].guide_material_volumes[0].start.x > 7200, "Shuffling does not mix materials or progression")
    generator.set_editor_property("hollow_regions", regions[:2])
    reject("Missing link")
    generator.set_editor_property("hollow_regions", [*regions, regions[0]])
    reject("Duplicate link")
    generator.set_editor_property("hollow_regions", [regions[0], None, regions[2]])
    reject("Empty reference")
    generator.set_editor_property("hollow_regions", regions)
    original_to = regions[2].to_room
    regions[2].set_editor_property("to_room", regions[0].from_room)
    reject("Nonconsecutive room link")
    regions[2].set_editor_property("to_room", original_to)
    original_width = regions[2].region_width
    original_chunk_length = generator.chunk_length
    generator.set_editor_property("chunk_length", 1400)
    original_above = regions[2].rock_above
    original_below = regions[2].rock_below
    regions[2].set_editor_property("region_width", 8000)
    regions[2].set_editor_property("rock_above", 3000)
    regions[2].set_editor_property("rock_below", 3000)
    reject("Last region exceeds 128 chunks after earlier candidates prepared")
    check("128" in generator.last_build_status, "Budget rejection comes from spatial chunk limit")
    regions[2].set_editor_property("region_width", original_width)
    regions[2].set_editor_property("rock_above", original_above)
    regions[2].set_editor_property("rock_below", original_below)
    generator.set_editor_property("chunk_length", original_chunk_length)
    original_chamber = chambers[2].get_actor_location()
    chambers[2].set_actor_location(original_chamber + unreal.Vector(0, 5000, 0), False, True)
    reject("Invalid cave in later region")
    chambers[2].set_actor_location(original_chamber, False, True)
    paths = [[region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(region.path.get_number_of_spline_points())] for region in regions]
    check(generator.add_missing_hollow_regions(), "Add missing is idempotent")
    check(len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowRegion)) == 3, "No duplicate authoring actors created")
    check(paths == [[region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(region.path.get_number_of_spline_points())] for region in regions], "Existing authored paths never refitted by add-missing")
    middle = regions[1].from_room
    original_location = middle.get_actor_location()
    middle.set_actor_location(original_location + unreal.Vector(100, 0, -30), False, True)
    check(generator.rebuild_tunnels(), "Moving a shared room updates both neighboring regions")
    middle.set_actor_location(original_location, False, True)
    for region, points in zip(regions, paths):
        region.path.set_spline_points(points, unreal.SplineCoordinateSpace.WORLD, True)
    check(generator.rebuild_tunnels(), "Final valid chain restored")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_chain_geometry.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_CHAIN_GEOMETRY " + json.dumps(report))
