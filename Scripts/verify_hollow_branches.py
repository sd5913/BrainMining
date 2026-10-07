import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowBranches_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = generator.hollow_region
chambers = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowChamber))
opened = next(chamber for chamber in chambers if not chamber.hidden_entrance)
hidden = next(chamber for chamber in chambers if chamber.hidden_entrance)
originals = [(chamber, chamber.get_actor_location(), unreal.Vector(chamber.chamber_size.x, chamber.chamber_size.y, chamber.chamber_size.z)) for chamber in chambers]
generator.set_editor_property("auto_rebuild", False)
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "build_ms": []}
temporary = []


def check(condition, name):
    assert condition, name
    report["checks"].append(name)


def trace(start, end, ignored=None):
    return unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored or [], unreal.DrawDebugTrace.NONE, True)


def hit_actor(hit):
    return hit.to_tuple()[9] if hit else None


def rebuild(name):
    start = time.perf_counter()
    check(generator.rebuild_tunnels(), name + ": " + generator.last_build_status)
    report["build_ms"].append((time.perf_counter() - start) * 1000)
    check(len(generator.generated_segments) == 1 and generator.space_count == sum(chamber.enabled for chamber in chambers), name + ": same progression segment")
    check(generator.segment_triangle_budget == 30000 and generator.generated_fill.largest_chunk_triangles <= 30000 and len(generator.generated_fill.chunks) <= 128, name + ": unchanged budgets")
    for chunk in generator.generated_fill.chunks:
        check(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()), name + ": watertight spatial chunk")


def reject(name):
    rock = generator.generated_fill
    revision = generator.build_revision
    triangles = rock.triangle_count
    snapshot = [(item.name, item.kind, tuple((point.x, point.y, point.z) for point in item.vertices)) for item in generator.generated_map_regions]
    check(not generator.rebuild_tunnels(), name + ": rejected")
    check(generator.generated_fill == rock and rock.triangle_count == triangles and generator.build_revision == revision, name + ": old geometry retained")
    check(snapshot == [(item.name, item.kind, tuple((point.x, point.y, point.z) for point in item.vertices)) for item in generator.generated_map_regions], name + ": map retains last successful snapshot")


try:
    rebuild("baseline")
    rock = generator.generated_fill
    for chamber in chambers:
        start = chamber.path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD)
        finish = chamber.path.get_location_at_spline_point(chamber.path.get_number_of_spline_points() - 1, unreal.SplineCoordinateSpace.WORLD)
        hit = trace(start, finish)
        check(hit_actor(hit) == rock if chamber.hidden_entrance else hit is None, chamber.get_actor_label() + ": entrance open/solid as designed")
        check(trace(start, finish, [rock]) is None, chamber.get_actor_label() + ": no internal bedrock sleeve")
        center = chamber.get_actor_location() + unreal.Vector(0, 0, 200)
        check(hit_actor(trace(center, center + unreal.Vector(600, 0, 0))) == rock, chamber.get_actor_label() + ": side wall belongs to ordinary rock")
    snapshot = list(generator.generated_map_regions)
    check(sum(item.kind == unreal.MiningMapRegionKind.SPACE for item in snapshot) == 2, "Two cave map regions")
    check(all(item.group_index == 0 for item in snapshot if item.kind != unreal.MiningMapRegionKind.ROOM), "Map ownership remains group zero")
    check(any(item.kind == unreal.MiningMapRegionKind.HIDDEN_ENTRANCE for item in snapshot), "Hidden entrance has developer marker")
    for chamber, location, size in originals:
        chamber.set_actor_location(location + unreal.Vector(70, 0, 0), False, True)
        rebuild("drag chamber")
        check(abs(chamber.path.get_location_at_spline_point(chamber.path.get_number_of_spline_points() - 1, unreal.SplineCoordinateSpace.WORLD).x - location.x - 70) < 0.1, "Branch endpoint follows dragged chamber")
        chamber.set_actor_location(location, False, True)
        chamber.set_editor_property("chamber_size", size + unreal.Vector(80, 80, 50))
        rebuild("resize chamber")
        chamber.set_editor_property("chamber_size", size)
        chamber.set_actor_scale3d(unreal.Vector(1.05, 1.05, 1.05))
        rebuild("scale chamber")
        chamber.set_actor_scale3d(unreal.Vector(1, 1, 1))
        chamber.reset_branch_path()
        check(chamber.manual_branch_path and chamber.path.get_number_of_spline_points() == 3, "Reset creates manual bend without losing connection")
        point = chamber.path.get_location_at_spline_point(1, unreal.SplineCoordinateSpace.WORLD)
        chamber.path.set_location_at_spline_point(1, point + unreal.Vector(35, 0, 0), unreal.SplineCoordinateSpace.WORLD, True)
        rebuild("manual branch bend")
        chamber.set_editor_property("manual_branch_path", False)
        chamber.set_actor_location(location + unreal.Vector(0, 5000, 0), False, True)
        reject("out of bounds")
        chamber.set_actor_location(location, False, True)
        chamber.set_actor_rotation(unreal.Rotator(yaw=10), False)
        reject("rotated chamber")
        chamber.set_actor_rotation(unreal.Rotator(), False)
    hidden.set_editor_property("entrance_thickness", 600)
    reject("seal overlaps cave")
    hidden.set_editor_property("entrance_thickness", 150)
    opened.set_editor_property("connection_position", 0.25)
    reject("branch overlaps main plug")
    opened.set_editor_property("connection_position", 0.14)
    opened.set_editor_property("enabled", False)
    rebuild("disabled open cave")
    check(not any(item.name.startswith(opened.get_actor_label()) for item in generator.generated_map_regions), "Disabled cave removed from map")
    opened.set_editor_property("enabled", True)
    rebuild("reenabled cave")
    rock = generator.generated_fill
    start = hidden.path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD)
    end = hidden.path.get_location_at_spline_point(1, unreal.SplineCoordinateSpace.WORLD)
    successful = 0
    for attempt in range(12):
        hit = trace(start, end)
        if not hit:
            break
        check(hit_actor(hit) == rock, "Hidden entrance contains only diggable rock")
        values = hit.to_tuple()
        check(rock.mine_sphere(values[5] - values[7] * 30, 160), "Mining hidden entrance succeeds")
        successful += 1
    check(trace(start, end) is None and successful > 0, "Digging opens hidden branch")
    rock.reset_rock()
    check(hit_actor(trace(start, end)) == rock, "Reset restores hidden entrance")
    report["hidden_cuts"] = successful
    anchor = region.path.get_location_at_distance_along_spline(region.path.get_spline_length() * 0.88, unreal.SplineCoordinateSpace.WORLD)
    extra = actors.spawn_actor_from_class(unreal.MiningHollowChamber, anchor + unreal.Vector(0, 1500, -region.passage_radius))
    temporary.append(extra)
    extra.set_actor_label("VALIDATE_EXTRA_CAVE")
    extra.set_editor_property("region", region)
    extra.set_editor_property("connection_position", 0.88)
    check(generator.rebuild_tunnels() and generator.space_count == 3, "Adding an owned chamber updates terrain")
    other_region = actors.spawn_actor_from_class(unreal.MiningHollowRegion, unreal.Vector())
    temporary.append(other_region)
    extra.set_editor_property("region", other_region)
    check(generator.rebuild_tunnels() and generator.space_count == 2, "Changing ownership removes cave from previous region")
    check(not any(item.name.startswith("VALIDATE_EXTRA_CAVE") for item in generator.generated_map_regions), "Other-region cave absent from this region map")
    extra.set_editor_property("region", region)
    check(generator.rebuild_tunnels() and generator.space_count == 3, "Restoring ownership restores cave")
    actors.destroy_actor(extra)
    temporary.remove(extra)
    check(generator.rebuild_tunnels() and generator.space_count == 2, "Deleting chamber removes terrain and map entry")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    for actor in temporary:
        actors.destroy_actor(actor)
    for chamber, location, size in originals:
        chamber.set_actor_location(location, False, True)
        chamber.set_actor_scale3d(unreal.Vector(1, 1, 1))
        chamber.set_actor_rotation(unreal.Rotator(), False)
        chamber.set_editor_property("chamber_size", size)
        chamber.set_editor_property("manual_branch_path", False)
        chamber.set_editor_property("enabled", True)
    hidden.set_editor_property("entrance_thickness", 150)
    opened.set_editor_property("connection_position", 0.14)
    assert generator.rebuild_tunnels(), generator.last_build_status
    generator.set_editor_property("auto_rebuild", True)
    generator.set_editor_property("section_position", 50)
    generator.show_section()
    assert levels.save_current_level()
    report["chunks"] = len(generator.generated_fill.chunks)
    report["largest_chunk"] = generator.generated_fill.largest_chunk_triangles
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_branches_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_BRANCHES_GEOMETRY " + json.dumps(report))
