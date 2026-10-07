import json
import time
import traceback
from pathlib import Path
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
originals = [(room.get_actor_transform(), room.room_size, room.layout_id) for room in rooms]
old_auto = generator.auto_rebuild
old_chunks = generator.automatic_chunks
query = unreal.GeometryScript_MeshQueries
report = {"passed": False, "checks": [], "segments": []}

def check(condition, label):
    assert condition, label
    report["checks"].append(label)

try:
    generator.set_editor_property("auto_rebuild", False)
    generator.set_editor_property("automatic_chunks", False)
    check(len(generator.generated_segments) == len(rooms) - 1, "Saved segmented actors reload")
    original_counts = [segment.triangle_count for segment in generator.generated_segments]
    for segment in generator.generated_segments:
        segment.reset_rock()
    check(original_counts == [segment.triangle_count for segment in generator.generated_segments], "Each saved reset mesh survives reload")
    for index, room in enumerate(rooms):
        room.set_actor_scale3d(unreal.Vector(1, 1, 1))
        room.set_actor_location(unreal.Vector(index * 9000, 0, 0), False, True)
        room.set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
    check(generator.rebuild_tunnels(), generator.last_build_status)
    segments = list(generator.generated_segments)
    started = time.monotonic()
    for index, segment in enumerate(segments):
        untouched = [other.triangle_count for other in segments if other != segment]
        max_ms = 0
        for cut in range(90):
            center = unreal.Vector(index * 9000 + 1000 + (cut % 30) * 220, 680, 200 + (cut // 30) * 400)
            check(segment.mine_sphere(center, 95), "Segment %d cut %d succeeds" % (index, cut))
            max_ms = max(max_ms, segment.last_total_milliseconds)
        check(untouched == [other.triangle_count for other in segments if other != segment], "Mining never edits another segment")
        check(query.get_is_closed_mesh(segment.rock_mesh.get_dynamic_mesh()), "Excavated segment remains closed")
        check(segment.triangle_count < segment.max_triangles, "Independent segment stays under its own budget")
        report["segments"].append({"cuts": segment.successful_cuts, "triangles": segment.triangle_count, "max_cut_ms": max_ms})
    total = sum(segment.triangle_count for segment in segments)
    check(total > 30000, "Combined geometry exceeds old 30000 shared limit while digging continues")
    report["total_triangles"] = total
    report["elapsed_seconds"] = time.monotonic() - started
    segments[0].max_triangles = 1000
    count = segments[0].triangle_count
    check(not segments[0].mine_sphere(unreal.Vector(7800, 680, 600), 95), "Per-segment protection still rejects over-budget cuts")
    check(segments[0].triangle_count == count, "Rejected cut preserves geometry")
    check(segments[1].mine_sphere(unreal.Vector(16800, 680, 600), 95), "A full segment does not block the next segment")
    revision = generator.build_revision
    identities = [segment.get_name() for segment in segments]
    rooms[1].set_actor_location(rooms[0].get_actor_location(), False, True)
    check(not generator.rebuild_tunnels(), "Invalid overlapping rooms rejected")
    check(generator.build_revision == revision and identities == [segment.get_name() for segment in generator.generated_segments], "Invalid edit retains all last-valid segment actors")
    rooms[1].set_actor_location(unreal.Vector(9000, 0, 0), False, True)
    for room in rooms[1:]:
        room.set_editor_property("layout_id", "SegmentTestExcluded")
    check(generator.rebuild_tunnels(), "Single-room layout succeeds")
    check(len(generator.generated_segments) == 0 and generator.generated_fill is None, "Single room leaves no ghost mining geometry")
    check(len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock)) == 0, "Removed segments are destroyed")
    generator.show_section()
    generator.show_layout()
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    for room, (transform, size, layout_id) in zip(rooms, originals):
        room.set_actor_transform(transform, False, True)
        room.set_editor_property("room_size", size)
        room.set_editor_property("layout_id", layout_id)
    generator.set_editor_property("automatic_chunks", old_chunks)
    report["restored"] = generator.rebuild_tunnels()
    generator.set_editor_property("auto_rebuild", old_auto)
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "segmented_geometry.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SEGMENTED_GEOMETRY " + json.dumps(report))
assert report["passed"], report.get("error")
