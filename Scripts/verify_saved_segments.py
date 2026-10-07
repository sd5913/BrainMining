import json
import re
from pathlib import Path
import unreal

output = Path(unreal.Paths.project_saved_dir()) / "Validation"
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
before = json.loads((output / "segmented_upgrade.json").read_text(encoding="utf-8"))["before"]

def stable(value):
    return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))

assert len(rooms) == len(before["rooms"])
for room, expected in zip(rooms, before["rooms"]):
    assert stable(room.get_actor_transform()) == stable(expected["transform"])
    assert stable(room.room_size) == stable(expected["size"])
assert [stable(generator.bedrock_material), stable(generator.tunnel_material)] == [stable(value) for value in before["materials"]]
assert [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity] == before["visibility"]
assert len(generator.generated_segments) == len(rooms) - 1
owned_segments = [segment for segment in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock) if segment.get_owner() == generator]
assert set(owned_segments) == set(generator.generated_segments), "Saved map contains untracked overlapping generated rocks"
assert generator.completed_segment_policy == unreal.CompletedMiningSegments.KEEP_FROZEN
assert generator.segment_triangle_budget == 30000
assert generator.player_scale_settings.enabled
assert abs(generator.player_scale_settings.scale - 0.7) < 0.0001
assert all(segment.triangle_count == (sum(chunk.initial_mesh.get_triangle_count() for chunk in segment.chunks) if segment.chunked else 12) for segment in generator.generated_segments)
report = {"passed": True, "scope": "Fresh process reloads the saved map without rebuilding", "room_layout_materials_lighting_preserved": True, "segments": len(generator.generated_segments), "owned_rocks": len(owned_segments), "player_scale": generator.player_scale_settings.scale}
(output / "segmented_saved_reload.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SEGMENTED_SAVED_RELOAD " + json.dumps(report))
