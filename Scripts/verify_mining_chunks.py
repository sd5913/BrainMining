import json
import math
import statistics
import time
import traceback
from pathlib import Path

import unreal


output = Path(unreal.Paths.project_saved_dir()) / "Validation"
output.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
originals = [(room.get_actor_transform(), room.room_size.copy(), room.layout_id) for room in rooms]
old_auto = generator.auto_rebuild
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "modes": {}}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def snapshot():
    return {
        "rooms": [{"transform": str(room.get_actor_transform()), "size": str(room.room_size), "layout": str(room.layout_id)} for room in rooms],
        "materials": [str(generator.bedrock_material), str(generator.tunnel_material)],
        "lights": [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity],
        "scale": str(generator.player_scale_settings),
        "budget": generator.segment_triangle_budget,
    }


def mesh_data(mesh):
    positions = query.get_all_vertex_positions(mesh, False)[1]
    triangles = query.get_all_triangle_indices(mesh, True)[1]
    return (
        [(value.x, value.y, value.z) for value in unreal.GeometryScript_List.convert_vector_list_to_array(positions)],
        [(value.x, value.y, value.z) for value in unreal.GeometryScript_List.convert_triangle_list_to_array(triangles)],
    )


def counts(segment):
    return [chunk.mesh.get_dynamic_mesh().get_triangle_count() for chunk in segment.chunks]


def times(values):
    ordered = sorted(values)
    return {"mean_ms": statistics.mean(values), "p95_ms": ordered[math.ceil(len(ordered) * 0.95) - 1], "max_ms": max(values)}


before = snapshot()
try:
    check(generator.segment_triangle_budget == 30000, "Budget remains 30000")
    generator.set_editor_property("auto_rebuild", False)
    generator.set_editor_property("automatic_chunks", True)
    check(generator.rebuild_tunnels(), generator.last_build_status)
    report["user_layout_chunks"] = [len(segment.chunks) for segment in generator.generated_segments]
    check(all(segment.chunked and len(segment.chunks) > 1 for segment in generator.generated_segments), "Saved user corridors automatically split")
    for segment in generator.generated_segments:
        for chunk in segment.chunks:
            check(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()), "User chunk is closed")
    for index, room in enumerate(rooms):
        if index > 1:
            room.set_editor_property("layout_id", "ChunkTestExcluded")
        else:
            room.set_actor_scale3d(unreal.Vector(1, 1, 1))
            room.set_actor_location(unreal.Vector(index * 21000, 0, 0), False, True)
            room.set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
    for mode, enabled, simplify, iterations in (("legacy", False, False, 100), ("chunks_only", True, False, 100), ("chunks_simplified", True, True, 360)):
        generator.set_editor_property("automatic_chunks", enabled)
        generator.set_editor_property("simplify_chunks", simplify)
        check(generator.rebuild_tunnels(), generator.last_build_status)
        segment = generator.generated_segments[0]
        durations = []
        simplification = []
        for cut in range(iterations):
            center = unreal.Vector(700 + (cut % 90) * 220, 680, 150 + (cut // 90) * 270)
            succeeded = segment.mine_sphere(center, 95)
            if not succeeded:
                if mode == "legacy" and "budget" in segment.last_status:
                    break
                raise AssertionError("%s cut %d: %s" % (mode, cut, segment.last_status))
            durations.append(segment.last_total_milliseconds)
            if enabled:
                check(segment.largest_chunk_triangles <= 30000, "Per-chunk budget respected")
                simplification.append(segment.last_simplification_milliseconds)
        report["modes"][mode] = {"cuts": segment.successful_cuts, "triangles": segment.triangle_count, "chunks": len(segment.chunks), "largest_chunk": segment.largest_chunk_triangles, "removed": segment.simplified_triangles, "matched_first_100": times(durations[:100]), "all": times(durations), "durations": durations}
        if mode == "chunks_simplified":
            check(segment.successful_cuts == iterations, "360 cuts succeed in ONE corridor")
            check(segment.triangle_count > 30000, "Single corridor exceeds old shared 30000 without raising any chunk budget")
            for chunk in segment.chunks:
                check(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()), "Excavated chunk remains closed")
            baseline = report["modes"]["legacy"]["matched_first_100"]
            actual = report["modes"][mode]["matched_first_100"]
            check(actual["mean_ms"] <= baseline["mean_ms"] * 1.1, "Mean mining time does not regress")
            check(actual["p95_ms"] <= baseline["p95_ms"] * 1.1, "P95 mining time does not regress")
    segment.reset_rock()
    for cut in range(60):
        check(segment.mine_sphere(unreal.Vector(700 + cut * 72, 350, 300 + math.sin(cut * 0.71) * 28), 160), "Overlapping excavation succeeds")
    report["overlapping_simplification"] = {"cuts": segment.successful_cuts, "triangles": segment.triangle_count, "removed": segment.simplified_triangles}
    check(segment.simplified_triangles > 0, "Controlled simplification removes redundant overlapping-cut geometry")
    segment.reset_rock()
    chunks = list(segment.chunks)
    first_bounds = chunks[0].bounds
    seam = first_bounds.max.x
    center = unreal.Vector(seam, 0, 600)
    untouched = [mesh_data(chunk.mesh.get_dynamic_mesh()) for chunk in chunks[2:]]
    check(segment.mine_sphere(center, 180), "Sphere centered exactly on seam succeeds")
    check(segment.last_touched_chunks == 2, "Both seam neighbors commit together")
    check(untouched == [mesh_data(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks[2:]], "Distant chunks remain byte-equivalent")
    seam_vertices = []
    for chunk in segment.chunks[:2]:
        vertices, _ = mesh_data(chunk.mesh.get_dynamic_mesh())
        seam_vertices.append({(round(vertex[1], 3), round(vertex[2], 3)) for vertex in vertices if abs(vertex[0] - seam) < 0.001 and abs(vertex[1]) < 181 and abs(vertex[2] - 600) < 181})
    check(len(seam_vertices[0]) >= 8 and seam_vertices[0] == seam_vertices[1], "Seam hole contours match across both chunks")
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(seam - 80, 0, 600), unreal.Vector(seam + 80, 0, 600), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    check(hit is None, "No collision wall remains in opened seam")
    for cut in range(14):
        check(segment.mine_sphere(unreal.Vector(seam + 250 + (cut % 3) * 200, 450, 180 + (cut // 3) * 170), 80), "Populate second chunk for atomic rollback")
    segment.max_triangles = 1000
    before_meshes = [mesh_data(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks]
    before_cuts = segment.successful_cuts
    check(not segment.mine_sphere(unreal.Vector(seam, 450, 400), 170), "One over-budget neighbor rejects entire seam cut")
    check(before_meshes == [mesh_data(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks] and segment.successful_cuts == before_cuts, "Rejected multi-chunk cut changes no geometry or counters")
    segment.max_triangles = 30000
    segment.reset_rock()
    check(segment.successful_cuts == 0 and segment.simplified_triangles == 0, "Reset clears counters and simplification state")
    check(all(mesh_data(chunk.mesh.get_dynamic_mesh()) == mesh_data(chunk.initial_mesh) for chunk in segment.chunks), "Reset restores every chunk exactly")
    identities = list(generator.generated_segments)
    generator.set_editor_property("chunk_length", 0)
    check(not generator.rebuild_tunnels() and list(generator.generated_segments) == identities, "Invalid chunk settings preserve last valid actors")
    generator.set_editor_property("chunk_length", 1200)
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    for room, (transform, size, layout_id) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
        room.set_editor_property("layout_id", layout_id)
    generator.set_editor_property("automatic_chunks", True)
    generator.set_editor_property("simplify_chunks", True)
    generator.set_editor_property("chunk_length", 1200)
    report["restored"] = generator.rebuild_tunnels()
    generator.set_editor_property("auto_rebuild", old_auto)
    report["before"] = before
    report["after"] = snapshot()
    if report["passed"] and report["restored"]:
        check(levels.save_current_level(), "Upgraded map saved")
    (output / "mining_chunks.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("MINING_CHUNKS " + json.dumps({key: value for key, value in report.items() if key not in ("checks", "before", "after", "modes")}))
assert report["passed"], report.get("error")
