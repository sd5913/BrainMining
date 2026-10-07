import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = generator.hollow_region
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "timings_ms": []}
original_points = [region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(region.path.get_number_of_spline_points())]
original_plugs = list(region.plugs)
original_width = region.region_width
original_radius = region.passage_radius
original_location = region.to_room.get_actor_location()
generator.set_editor_property("auto_rebuild", False)


def check(condition, name):
    assert condition, name
    report["checks"].append(name)


def trace(start, end, ignored=None):
    return unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*end), unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored or [], unreal.DrawDebugTrace.NONE, True)


def hit_actor(hit):
    return hit.to_tuple()[9] if hit else None


def rebuild(name):
    started = time.perf_counter()
    check(generator.rebuild_tunnels(), name + ": " + generator.last_build_status)
    report["timings_ms"].append({"name": name, "milliseconds": (time.perf_counter() - started) * 1000})
    check(query.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), name + ": closed bedrock")
    check(len(generator.generated_segments) == 1, name + ": one progression group")
    check(generator.segment_triangle_budget == 30000 and generator.generated_fill.largest_chunk_triangles <= 30000, name + ": original budget retained")
    for chunk in generator.generated_fill.chunks:
        assert query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh())


def rejected(name):
    segment = generator.generated_fill
    triangles = segment.triangle_count
    revision = generator.build_revision
    check(not generator.rebuild_tunnels(), name + ": rejected")
    check(generator.generated_fill == segment and segment.triangle_count == triangles and generator.build_revision == revision, name + ": previous geometry retained")


try:
    check(len(region.plugs) == 3 and len(original_points) == 5, "Saved editor controls survive reload")
    check(len(generator.generated_fill.guide_material_volumes) == 6, "Guide masks survive reload")
    rebuild("baseline")
    rock = generator.generated_fill
    check(trace((1000, 40, 290), (1600, 100, 215)) is None, "Main passage is already empty")
    check(hit_actor(trace((1700, 130, 200), (2200, 130, 200))) == rock, "First plug blocks passage and is diggable")
    check(hit_actor(trace((3600, 0, -50), (3600, 1100, -50), [rock])) is None, "No internal bedrock sleeve")
    check(hit_actor(trace((3000, 0, 30), (3000, 1000, 30))) == rock, "Passage side wall is ordinary mineable rock")
    check(hit_actor(trace((3000, 1000, 30), (3000, 1600, 30), [rock])) == generator.generated_bedrock, "Only the global boundary is bedrock")
    check(hit_actor(trace((0, 0, 250), (1000, 0, 250))) is None, "Start room portal is open")
    check(hit_actor(trace((6200, 0, -500), (7200, 0, -500))) is None, "Destination room portal is open")
    kinds = [item.kind for item in generator.generated_map_regions]
    check(unreal.MiningMapRegionKind.ROCK_REGION in kinds and unreal.MiningMapRegionKind.GUIDE_PLUG in kinds, "Developer map has rock, passage and plug snapshots")
    check(rock.mine_sphere(unreal.Vector(3600, 0, -50), 180), "Mining works across spatial chunk seams")
    check(rock.last_touched_chunks >= 2 and rock.successful_cuts == 1, "Cross-chunk cut commits atomically")
    rock.reset_rock()
    check(rock.successful_cuts == 0 and rock.triangle_count == sum(chunk.initial_mesh.get_triangle_count() for chunk in rock.chunks), "Reset restores all spatial chunks")
    check(rock.mine_sphere(unreal.Vector(3000, 420, 30), 180), "Ordinary side rock is diggable too")
    rock.reset_rock()
    region.set_editor_property("region_width", 3200)
    rebuild("wider region")
    check(region.resolved_size.y == 3200, "Region size is editable")
    region.set_editor_property("region_width", original_width)
    region.set_editor_property("passage_radius", 300)
    rebuild("smaller circular passage")
    region.set_editor_property("passage_radius", original_radius)
    region.path.set_spline_points(original_points, unreal.SplineCoordinateSpace.WORLD, True)
    point = original_points[2] + unreal.Vector(0, 220, 0)
    region.path.set_location_at_spline_point(2, point, unreal.SplineCoordinateSpace.WORLD, True)
    rebuild("hand-edited path")
    region.to_room.set_actor_location(original_location + unreal.Vector(400, 0, -100), False, True)
    rebuild("moved destination room")
    check(abs(region.path.get_location_at_spline_point(4, unreal.SplineCoordinateSpace.WORLD).x - 7000) < 1, "Endpoint follows destination wall")
    region.to_room.set_actor_location(original_location, False, True)
    region.path.set_spline_points(original_points, unreal.SplineCoordinateSpace.WORLD, True)
    region.set_editor_property("plugs", [])
    rebuild("no plugs")
    check(not generator.generated_fill.guide_material_volumes, "Deleting plugs removes their guide masks")
    region.set_editor_property("plugs", original_plugs)
    rebuild("restored plugs")
    region.set_editor_property("passage_radius", 700)
    rejected("oversized portal")
    region.set_editor_property("passage_radius", original_radius)
    generator.set_editor_property("chunk_length", 400)
    rejected("over 128 spatial chunks")
    generator.set_editor_property("chunk_length", 1600)
    region.path.set_location_at_spline_point(2, original_points[1], unreal.SplineCoordinateSpace.WORLD, True)
    rejected("backtracking path")
    region.path.set_spline_points(original_points, unreal.SplineCoordinateSpace.WORLD, True)
    rebuild("final restore")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    region.set_editor_property("plugs", original_plugs)
    region.set_editor_property("region_width", original_width)
    region.set_editor_property("passage_radius", original_radius)
    region.to_room.set_actor_location(original_location, False, True)
    region.path.set_spline_points(original_points, unreal.SplineCoordinateSpace.WORLD, True)
    generator.set_editor_property("chunk_length", 1600)
    assert generator.rebuild_tunnels(), generator.last_build_status
    generator.set_editor_property("auto_rebuild", True)
    generator.set_editor_property("section_keep_positive", False)
    generator.show_section()
    for light in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight):
        light.set_actor_rotation(unreal.Rotator(pitch=-50, yaw=-135, roll=0), True)
    editor.set_level_viewport_camera_info(unreal.Vector(3600, 6500, 1000), unreal.Rotator(pitch=-10, yaw=-90, roll=0))
    assert levels.save_current_level()
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_region_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_REGION_GEOMETRY " + json.dumps(report))
