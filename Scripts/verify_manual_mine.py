import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualRegion)[0]
shapes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualShape)
main = region.main_passage
side = next(shape for shape in shapes if shape.kind == unreal.MiningManualKind.SIDE_ROOM)
branch = next(shape for shape in shapes if shape.kind == unreal.MiningManualKind.PASSAGE and shape != main)
hidden = next(shape for shape in shapes if shape.hidden_cavity)
generator.set_editor_property("auto_rebuild", False)
report = {"passed": False, "checks": []}


def check(condition, name):
    assert condition, name
    report["checks"].append(name)


def snapshot():
    return {"revision": generator.build_revision, "segments": [item.get_path_name() for item in generator.generated_segments], "triangles": [item.triangle_count for item in generator.generated_segments], "bedrock": generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count(), "map": [item.export_text() for item in generator.generated_map_regions]}


def reject(name, reason=None):
    before = snapshot()
    check(not generator.rebuild_tunnels(), name + ": rejected")
    if reason:
        check(reason in generator.last_build_status, name + ": exact validation reason: " + generator.last_build_status)
    check(before == snapshot(), name + ": last playable geometry, collision owner and map retained")


def trace(start, end, ignore=None):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*end), unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignore or [], unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[9] if hit else None


try:
    check(len(shapes) == 7 and generator.room_count == 2 and generator.space_count == 6, "All manual authoring and generated references survive reload")
    authored = {shape.get_name(): ([point.export_text() for point in shape.boundary_a], [point.export_text() for point in shape.boundary_b]) for shape in shapes}
    check(generator.rebuild_tunnels(), generator.last_build_status)
    check(authored == {shape.get_name(): ([point.export_text() for point in shape.boundary_a], [point.export_text() for point in shape.boundary_b]) for shape in shapes}, "Rebuild never rewrites source points or tangents")
    segment = generator.generated_segments[0]
    check(segment.max_triangles == 30000 and len(segment.chunks) <= 128, "Original 30000/128 budgets unchanged")
    check(all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks), "Every spatial chunk is closed")
    check(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), "Outer bedrock is closed")
    check(trace((450, 0, 500), (850, 0, 500)) is None, "Main entry opens through room/region bedrock")
    check(trace((6400, 0, -300), (6750, 0, -300)) is None, "Main exit opens through bedrock")
    check(trace((4600, 0, -1000), (4600, 0, -1250)) is None, "Branch portal is connected to side room")
    check(trace((1500, 0, -1450), (1500, 0, -1050)) is None, "Hidden cavity has empty interior")
    check(trace((1500, 0, -1200), (1500, 0, -900)) == segment, "Hidden cavity is surrounded by ordinary mineable rock")
    check(trace((2400, 0, 700), (2400, 1000, 700), [segment]) is None, "No bedrock sleeve around caves or passages")
    check(trace((2400, 1000, 700), (2400, 1450, 700), [segment]) == generator.generated_bedrock, "Outer region border remains indestructible bedrock")
    check(trace((5300, 0, 1300), (5900, 0, 1300)) is None, "Concave cave upper arm is hollow")
    check(trace((5500, 0, 1300), (5500, 0, 1075)) == segment, "Concave notch is rock, not a convex hull cavity")
    check(trace((5900, 0, 1075), (5900, -800, 1075)) == segment, "Cavity sidewall remains mineable")
    check(trace((3000, 0, -100), (3400, 0, -100)) == segment, "Hand-authored rock block closes the passage")
    check(any(item.kind == unreal.MiningMapRegionKind.BRANCH for item in generator.generated_map_regions), "Side room uses non-progression minimap type")
    check(any(len(item.get_editor_property("PolygonXZ")) == 8 for item in generator.generated_map_regions), "Minimap serializes exact concave outline")
    old_floor = side.get_actor_location()
    side.set_actor_location(old_floor + unreal.Vector(100, 0, 50), False, False)
    check(generator.rebuild_tunnels(), "Moved side room still connects: " + generator.last_build_status)
    check([point.export_text() for point in branch.boundary_a] == authored[branch.get_name()][0], "Room movement follows portal without replacing middle nodes")
    check(trace((4700, 0, -950), (4700, 0, -1200)) is None, "Moved room portal collision follows authored room")
    side.set_actor_location(old_floor, False, False)
    check(generator.rebuild_tunnels(), "Side room restored")
    original_main = [point.export_text() for point in main.boundary_a]
    original_leave = (main.boundary_a[2].leave.x, main.boundary_a[2].leave.y)
    changed = list(main.boundary_a)
    changed[2].set_editor_property("leave", unreal.Vector2D(350, 150))
    main.set_editor_property("boundary_a", changed)
    check(generator.rebuild_tunnels(), "Changed curve handles create real geometry")
    restored = list(main.boundary_a)
    restored[2].set_editor_property("leave", unreal.Vector2D(*original_leave))
    main.set_editor_property("boundary_a", restored)
    check(generator.rebuild_tunnels(), "Authored handles restored")
    check([point.export_text() for point in main.boundary_a] == original_main, "Restored handles match original source values exactly")
    original = list(hidden.boundary_a)
    hidden.set_editor_property("boundary_a", [original[0], original[2], original[1], original[3]])
    reject("Crossed contour")
    hidden.set_editor_property("boundary_a", original)
    check(generator.rebuild_tunnels(), "Recover after invalid draft")
    old_location = hidden.get_actor_location()
    hidden.set_actor_location(unreal.Vector(9000, 0, 0), False, False)
    reject("Space outside editable region")
    hidden.set_actor_location(old_location, False, False)
    original_aperture = branch.end_portal.aperture
    invalid_portal = branch.end_portal
    invalid_portal.set_editor_property("aperture", 3000)
    branch.set_editor_property("end_portal", invalid_portal)
    reject("Portal too wide for room")
    restored_portal = branch.end_portal
    restored_portal.set_editor_property("aperture", original_aperture)
    branch.set_editor_property("end_portal", restored_portal)
    check(generator.rebuild_tunnels(), "Valid portal recovered: " + generator.last_build_status)
    region.set_editor_property("rock_size", unreal.Vector(6000, 7000, 7000))
    reject("Chunk count over original budget", "128")
    region.set_editor_property("rock_size", unreal.Vector(6000, 2400, 3600))
    check(generator.rebuild_tunnels(), "Original valid layout recovered: " + generator.last_build_status)
    segment = generator.generated_segments[0]
    initial = segment.triangle_count
    check(segment.mine_sphere(unreal.Vector(3200, 0, -100), 230), "Authored block supports real chunk mining")
    check(segment.successful_cuts > 0 and segment.triangle_count != initial, "Mining changes geometry within budget")
    segment.reset_rock()
    check(segment.triangle_count == initial, "Reset restores authored geometry")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "manual_geometry.json"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MANUAL_GEOMETRY " + json.dumps(report))
