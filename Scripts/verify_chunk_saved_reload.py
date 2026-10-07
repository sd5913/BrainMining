import json
import re
from pathlib import Path

import unreal


output = Path(unreal.Paths.project_saved_dir()) / "Validation"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
baseline = json.loads((output / "mining_chunks.json").read_text(encoding="utf-8"))["before"]


def stable(value):
    return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))


assert len(rooms) == len(baseline["rooms"])
for room, expected in zip(rooms, baseline["rooms"]):
    assert stable(room.get_actor_transform()) == stable(expected["transform"])
    assert stable(room.room_size) == stable(expected["size"])
assert [stable(generator.bedrock_material), stable(generator.tunnel_material)] == [stable(value) for value in baseline["materials"]]
assert [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity] == baseline["lights"]
assert stable(generator.player_scale_settings) == stable(baseline["scale"])
assert generator.segment_triangle_budget == baseline["budget"] == 30000
assert generator.automatic_chunks and generator.simplify_chunks
assert len(generator.generated_segments) == len(rooms) - 1
owned = [actor for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock) if actor.get_owner() == generator]
assert set(owned) == set(generator.generated_segments)
for segment in generator.generated_segments:
    assert segment.chunked and segment.chunks
    assert segment.rock_mesh.get_dynamic_mesh().get_triangle_count() == 0
    assert segment.rock_mesh.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
    expected = sum(chunk.initial_mesh.get_triangle_count() for chunk in segment.chunks)
    assert expected == sum(chunk.mesh.get_dynamic_mesh().get_triangle_count() for chunk in segment.chunks)
    segment.reset_rock()
    assert segment.triangle_count == expected
    assert all(chunk.mesh.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for chunk in segment.chunks)
report = {"passed": True, "scope": "Fresh process reload without generation", "rooms": len(rooms), "groups": len(owned), "chunks": [len(segment.chunks) for segment in owned], "layout_materials_lights_scale_preserved": True, "budget": generator.segment_triangle_budget}
(output / "chunk_saved_reload.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("CHUNK_SAVED_RELOAD " + json.dumps(report))
