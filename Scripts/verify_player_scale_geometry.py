import json
import runpy
from pathlib import Path

import unreal


runpy.run_path(str(Path(__file__).parent / "verify_direct_faces.py"))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
orphan = actors.spawn_actor_from_class(unreal.GeneratedMiningRock, generator.get_actor_location())
orphan.set_owner(generator)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
order = rooms[1].connection_order
rooms[1].set_editor_property("connection_order", rooms[0].connection_order)
before = list(generator.generated_segments)
assert not generator.rebuild_tunnels()
assert list(generator.generated_segments) == before
assert orphan in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock)
rooms[1].set_editor_property("connection_order", order)
assert generator.rebuild_tunnels()
owned = [segment for segment in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock) if segment.get_owner() == generator]
assert set(owned) == set(generator.generated_segments)
assert orphan not in owned
report = {"passed": True, "direct_geometry_regression": True, "failed_rebuild_preserves_actors": True, "successful_rebuild_removes_owned_orphans": True, "owned_segments": len(owned)}
(Path(unreal.Paths.project_saved_dir()) / "Validation" / "player_scale_geometry.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("PLAYER_SCALE_GEOMETRY " + json.dumps(report))
