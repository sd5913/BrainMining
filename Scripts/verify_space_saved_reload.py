import hashlib
import json
from pathlib import Path

import unreal


root = Path(unreal.Paths.project_dir())
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
spaces = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace)
assert len(spaces) == 3 and generator.space_count == 3
assert generator.segment_triangle_budget == 30000
assert all(space.from_room is not None and space.get_editor_property("preview_material") is not None for space in spaces)
assert all(space.content_root.get_editor_property("absolute_scale") for space in spaces)
assert all(space.get_editor_property("preview").get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for space in spaces)
lights = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PointLight) if "_ContentLight" in actor.get_actor_label()]
assert len(lights) == 2 and all(actor.get_attach_parent_actor() in spaces for actor in lights)
counts = [segment.triangle_count for segment in generator.generated_segments]
for segment in generator.generated_segments:
    assert segment.chunked
    assert sum(chunk.initial_mesh.get_triangle_count() for chunk in segment.chunks) == segment.triangle_count
    segment.reset_rock()
assert counts == [segment.triangle_count for segment in generator.generated_segments]
hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(4500, 1500, 200), unreal.Vector(4500, 2300, 200), unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
assert hit is not None and isinstance(hit.to_tuple()[9], unreal.GeneratedMiningRock)
assert unreal.SystemLibrary.line_trace_single(world, unreal.Vector(4500, 1500, 200), unreal.Vector(4500, 2300, 200), unreal.TraceTypeQuery.ECC_VISIBILITY, True, list(generator.generated_segments), unreal.DrawDebugTrace.NONE, True) is None
original = root / "Content/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms.umap"
backup = root / "Saved/RoomDrivenBackup/Spaces_2026-10-05_21-46-51/Lvl_MiningDescent_AutoRooms.umap"
assert hashlib.sha256(original.read_bytes()).digest() == hashlib.sha256(backup.read_bytes()).digest()
report = {"passed": True, "scope": "Fresh process, no regeneration; saved space references, props, meshes and collision", "spaces": len(spaces), "chunks": [len(segment.chunks) for segment in generator.generated_segments], "triangles": counts, "original_map_sha256_unchanged": True}
(root / "Saved/Validation/space_saved_reload.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SPACE_SAVED_RELOAD " + json.dumps(report))
