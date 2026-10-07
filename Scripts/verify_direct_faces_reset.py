import json
import re
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None, "Run after the runtime digging test, before stopping PIE"
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
pawn = unreal.GameplayStatics.get_player_character(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
runtime = json.loads((output / globals().get("runtime_output", "direct_faces_runtime.json")).read_text(encoding="utf-8"))
cuts_before_reset = sum(segment.successful_cuts for segment in generator.generated_segments)
mining.reset_field()
assert [segment.triangle_count for segment in generator.generated_segments] == runtime["initial_triangles"]
assert all(segment.successful_cuts == 0 for segment in generator.generated_segments)
assert generator.current_room_index == 0 and generator.active_segment_index == 0
capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
expected_spawn = generator.generated_player_start.get_actor_location()
expected_spawn.z += capsule.get_scaled_capsule_half_height() * (1 - 1 / pawn.applied_player_scale)
assert (pawn.get_actor_location() - expected_spawn).length() < 20
original = json.loads((output / "segmented_upgrade.json").read_text(encoding="utf-8"))["before"]["rooms"]
editor_rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
for room, expected in zip(editor_rooms, original):
    for actual, key in ((room.get_actor_transform(), "transform"), (room.room_size, "size")):
        assert re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(actual)) == re.sub(r" \(0x[0-9A-Fa-f]+\)", "", expected[key])
assert len(editor_rooms) == len(original)
assert not isinstance(generator.generated_bedrock, unreal.DiggableRock)
report = {"passed": True, "cuts_before_reset": cuts_before_reset, "restored_triangles": [segment.triangle_count for segment in generator.generated_segments], "player_reset": True, "user_room_layout_preserved": True, "bedrock_not_diggable": True}
(output / globals().get("reset_output", "direct_faces_reset.json")).write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("DIRECT_FACES_RESET " + json.dumps(report))
