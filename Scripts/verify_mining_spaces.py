import json
import time
import traceback
from pathlib import Path

import unreal


output = Path(unreal.Paths.project_saved_dir()) / "Validation"
output.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
originals = [(room.get_actor_transform(), room.room_size.copy(), room.layout_id) for room in rooms]
original_spaces = [(space, space.enabled) for space in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace)]
old_auto = generator.auto_rebuild
query = unreal.GeometryScript_MeshQueries
spawned = []
report = {"passed": False, "checks": [], "timings": []}


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def trace(start, finish, ignored=None):
    return unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*finish), unreal.TraceTypeQuery.ECC_VISIBILITY, True, ignored or [], unreal.DrawDebugTrace.NONE, True)


def rebuild(label):
    started = time.perf_counter()
    check(generator.rebuild_tunnels(), label + ": " + generator.last_build_status)
    report["timings"].append({"label": label, "milliseconds": (time.perf_counter() - started) * 1000})
    check(query.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh()), label + ": closed bedrock")
    check(generator.segment_triangle_budget == 30000, label + ": unchanged budget")
    for segment in generator.generated_segments:
        check(segment.largest_chunk_triangles <= 30000, label + ": initial chunks within budget")
        for chunk in segment.chunks:
            check(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()), label + ": closed chunk")


def rejected(label):
    segments = list(generator.generated_segments)
    triangles = [segment.triangle_count for segment in segments]
    bedrock_triangles = generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count()
    revision = generator.build_revision
    check(not generator.rebuild_tunnels(), label + ": edit rejected")
    check(list(generator.generated_segments) == segments and [segment.triangle_count for segment in segments] == triangles, label + ": old segments preserved")
    check(generator.build_revision == revision and generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh().get_triangle_count() == bedrock_triangles, label + ": bedrock/revision preserved")


def spawn_space(location, size):
    space = actors.spawn_actor_from_class(unreal.MiningLayoutSpace, unreal.Vector(*location))
    spawned.append(space)
    space.set_editor_property("from_room", rooms[0])
    space.set_editor_property("align_branch_floors", False)
    space.set_editor_property("space_size", unreal.Vector(*size))
    return space


try:
    generator.set_editor_property("auto_rebuild", False)
    for space, enabled in original_spaces:
        space.set_editor_property("enabled", False)
    rebuild("user layout without spaces")
    report["baseline_chunks"] = [len(segment.chunks) for segment in generator.generated_segments]
    for index, room in enumerate(rooms):
        if index > 2:
            room.set_editor_property("layout_id", "SpaceTestExcluded")
        else:
            room.set_actor_scale3d(unreal.Vector(1, 1, 1))
            room.set_actor_location(unreal.Vector(index * 9000, 0, 0), False, True)
            room.set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
    rebuild("test layout")
    baseline = [segment.triangle_count for segment in generator.generated_segments]
    space = spawn_space((4500, 0, 100), (1200, 2400, 1000))
    rebuild("empty inline expansion")
    check(generator.space_count == 1 and generator.room_count == 3 and len(generator.generated_segments) == 2, "Spaces do not create progression rooms or groups")
    check(trace((4500, -1100, 600), (4500, 1100, 600)) is None, "Expanded cavity has no old rock or bedrock partition")
    check(trace((4500, 1000, 600), (4500, 1500, 600), list(generator.generated_segments)) is not None, "Expanded cavity has an outer bedrock wall")
    check(generator.generated_segments[1].triangle_count == baseline[1], "Unrelated tunnel geometry preserved")
    space.set_editor_property("contents", unreal.MiningSpaceContents.DIGGABLE_ROCK)
    rebuild("diggable inline expansion")
    check(trace((4500, 1500, 600), (4500, 1000, 600), [generator.generated_bedrock]) is not None, "Diggable expansion is actually filled with mineable geometry")
    space.set_editor_property("contents", unreal.MiningSpaceContents.EMPTY)
    space.set_editor_property("shape", unreal.MiningSpaceShape.ELLIPSOID)
    rebuild("ellipsoid inline expansion")
    check(trace((4500, -1000, 600), (4500, 1000, 600)) is None, "Ellipsoid creates an empty expanded center")
    space.set_editor_property("shape", unreal.MiningSpaceShape.BOX)
    space.set_editor_property("space_size", unreal.Vector(1200, 1200, 1000))
    seam = 500 + 8000 * 4 / 7
    space.set_actor_location(unreal.Vector(seam, 2600, 100), False, True)
    rejected("disconnected overlap")
    space.set_editor_property("connection", unreal.MiningSpaceConnection.STRAIGHT_BRANCH)
    rebuild("open straight branch")
    check(trace((seam, 10, 600), (seam, 2900, 600)) is None, "Open branch connects through old bedrock into cave")
    space.set_editor_property("entrance", unreal.MiningSpaceEntrance.ROCK_FILLED)
    rebuild("rock filled branch")
    check(trace((seam, 2300, 600), (seam, 1600, 600), [generator.generated_bedrock]) is not None, "Rock-filled branch remains discoverable by digging")
    space.set_editor_property("entrance", unreal.MiningSpaceEntrance.HIDDEN_WALL)
    rebuild("hidden wall across chunk seam")
    check(trace((seam, 10, 600), (seam, 1840, 600)) is None, "Hidden branch open before retained wall")
    wall_hit = trace((seam, 1700, 600), (seam, 2100, 600))
    check(wall_hit is not None, "Hidden entrance has a physical rock wall")
    check(abs(wall_hit.to_tuple()[5].y - 1880) < 1, "Hidden wall starts at requested centerline thickness")
    check(trace((seam, 1700, 600), (seam, 2800, 600), list(generator.generated_segments)) is None, "Hidden entrance has no indestructible divider")
    segment = generator.generated_segments[0]
    initial = segment.triangle_count
    check(segment.mine_sphere(unreal.Vector(seam, 1940, 600), 175), segment.last_status)
    check(segment.last_touched_chunks >= 2, "One hidden-wall cut updates both seam chunks")
    check(trace((seam, 1700, 600), (seam, 2800, 600)) is None, "Mining opens actual passage into hidden cave")
    segment.reset_rock()
    check(segment.triangle_count == initial and trace((seam, 1700, 600), (seam, 2100, 600)) is not None, "Reset restores hidden wall and original chunk meshes")
    space.set_editor_property("entrance_rock_thickness", 220)
    rebuild("thicker hidden wall")
    check(abs(trace((seam, 1700, 600), (seam, 2100, 600)).to_tuple()[5].y - 1780) < 1, "Wall thickness is editable")
    space.set_editor_property("shape", unreal.MiningSpaceShape.ELLIPSOID)
    rebuild("ellipsoid hidden cave")
    check(trace((seam, 1700, 600), (seam, 2800, 600), list(generator.generated_segments)) is None, "Ellipsoid branch is clear of bedrock")
    space.set_editor_property("align_branch_floors", True)
    space.set_editor_property("shape", unreal.MiningSpaceShape.BOX)
    space.set_editor_property("entrance", unreal.MiningSpaceEntrance.OPEN)
    space.set_actor_location(unreal.Vector(seam, 2600, 0), False, True)
    rebuild("floor aligned branch")
    check(trace((seam, 10, 100), (seam, 2900, 100)) is None, "Default branch floor has no entrance step in a level box cave")
    floor_hit = trace((seam, 1400, 100), (seam, 1400, -100))
    check(floor_hit is not None and abs(floor_hit.to_tuple()[5].z) < 1, "Branch floor aligns with main tunnel floor")
    space.set_actor_location(unreal.Vector(seam, 2600, -250), False, True)
    rebuild("sloping floor aligned branch")
    check(trace((seam, 10, 170), (seam, 2400, -60)) is None, "Sloping upright branch center remains clear")
    space.set_editor_property("align_branch_floors", False)
    space.set_actor_rotation(unreal.Rotator(0, 15, 0), False)
    rejected("rotated space")
    space.set_actor_rotation(unreal.Rotator(), False)
    space.set_actor_scale3d(unreal.Vector(-1, 1, 1))
    rejected("negative space scale")
    space.set_actor_scale3d(unreal.Vector(1, 1, 1))
    space.set_editor_property("from_room", rooms[2])
    rejected("last room has no outgoing tunnel")
    space.set_editor_property("from_room", rooms[0])
    space.set_actor_location(unreal.Vector(9000, 0, 100), False, True)
    rejected("main room intersection")
    space.set_actor_location(unreal.Vector(13000, 2600, 100), False, True)
    rejected("wrong tunnel crossing")
    space.set_editor_property("connection", unreal.MiningSpaceConnection.OVERLAP)
    space.set_editor_property("shape", unreal.MiningSpaceShape.BOX)
    space.set_actor_location(unreal.Vector(4500, 0, 100), False, True)
    space.set_editor_property("space_size", unreal.Vector(1200, 2400, 1000))
    other = spawn_space((4600, 1400, 200), (1000, 1000, 750))
    rebuild("overlapping caves connected as chain")
    check(trace((4600, 500, 600), (4600, 1700, 600)) is None, "Same-tunnel overlapping spaces merge without internal walls")
    other.set_editor_property("contents", unreal.MiningSpaceContents.DIGGABLE_ROCK)
    rebuild("empty precedence over overlapping fill")
    check(trace((4600, 500, 600), (4600, 1100, 600)) is None, "Overlapping filled space cannot refill another empty cavity")
    prop = actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(4600, 1500, 400))
    spawned.append(prop)
    prop.attach_to_component(other.content_root, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    prop_before = prop.get_actor_location()
    other.set_actor_location(other.get_actor_location() + unreal.Vector(0, 50, 0), False, True)
    check((prop.get_actor_location() - prop_before - unreal.Vector(0, 50, 0)).length() < 0.1, "Attached content follows space movement")
    prop_position = prop.get_actor_location()
    prop_scale = prop.get_actor_scale3d()
    other.set_editor_property("space_size", unreal.Vector(1100, 1000, 800))
    rebuild("content preserved during resize")
    check((prop.get_actor_location() - prop_position).length() < 0.01 and (prop.get_actor_scale3d() - prop_scale).length() < 0.01, "Dimensions and terrain rebuild do not transform or destroy content")
    other.set_actor_scale3d(unreal.Vector(1.1, 1, 1))
    check((prop.get_actor_scale3d() - unreal.Vector(1, 1, 1)).length() < 0.01, "Space actor scaling does not stretch ContentRoot props")
    other.set_editor_property("enabled", False)
    space.set_editor_property("enabled", False)
    rebuild("disabled spaces")
    check(generator.space_count == 0 and [segment.triangle_count for segment in generator.generated_segments] == baseline, "Disable restores unmodified base terrain")
    actors.destroy_actor(other)
    spawned.remove(other)
    actors.destroy_actor(space)
    spawned.remove(space)
    rebuild("deleted spaces")
    check([segment.triangle_count for segment in generator.generated_segments] == baseline, "Delete restores unmodified base terrain")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    for actor in reversed(spawned):
        if unreal.SystemLibrary.is_valid(actor):
            actors.destroy_actor(actor)
    for room, saved in zip(rooms, originals):
        room.set_actor_transform(saved[0], False, True)
        room.set_editor_property("room_size", saved[1])
        room.set_editor_property("layout_id", saved[2])
    for space, enabled in original_spaces:
        space.set_editor_property("enabled", enabled)
    report["restored"] = generator.rebuild_tunnels()
    generator.set_editor_property("auto_rebuild", old_auto)
    report["passed"] = report["passed"] and report["restored"]
    (output / "mining_spaces.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("MINING_SPACES " + json.dumps(report))
if not report["passed"]:
    raise AssertionError(report.get("error", "Restoration failed"))
