import json
import re
import runpy
import time
import traceback
from pathlib import Path
import unreal

output = Path(unreal.Paths.project_saved_dir()) / "Validation"
started = time.time()
runpy.run_path(str(Path(__file__).parent / "verify_auto_rooms_editor.py"))

def stable(value):
    return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))

def finalize(delta):
    path = output / "auto_rooms_editor.json"
    if not path.exists() or path.stat().st_mtime < started:
        return
    unreal.unregister_slate_post_tick_callback(final_callback)
    report = {"passed": False}
    try:
        edits = json.loads(path.read_text(encoding="utf-8"))
        assert edits["passed"], edits["reason"]
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        assert editor.get_game_world() is None
        world = editor.get_editor_world()
        generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
        rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
        before = json.loads((output / "segmented_upgrade.json").read_text(encoding="utf-8"))["before"]
        assert len(rooms) == len(before["rooms"])
        for room, expected in zip(rooms, before["rooms"]):
            assert stable(room.get_actor_transform()) == stable(expected["transform"])
            assert stable(room.room_size) == stable(expected["size"])
        assert [stable(generator.bedrock_material), stable(generator.tunnel_material)] == [stable(value) for value in before["materials"]]
        assert [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity] == before["visibility"]
        assert [[str(route.start_wall), str(route.end_wall)] for route in generator.routes] == before["faces"]
        assert str(generator.preview_mode) == before["preview"]
        assert generator.completed_segment_policy == unreal.CompletedMiningSegments.KEEP_FROZEN
        assert generator.auto_rebuild and generator.layout_applied
        assert len(generator.generated_segments) == len(rooms) - 1
        assert all(segment.triangle_count == 12 and segment.successful_cuts == 0 for segment in generator.generated_segments)
        assert world.get_outer() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Room restoration left the map unsaved"
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([generator])
        report.update({"passed": True, "room_layout_preserved": True, "material_lighting_faces_preview_preserved": True, "segments": len(generator.generated_segments), "default_policy": str(generator.completed_segment_policy), "auto_rebuild": generator.auto_rebuild, "ready_to_play": True})
    except Exception:
        report["error"] = traceback.format_exc()
    (output / "segmented_final_editor.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SEGMENTED_FINAL_EDITOR " + json.dumps(report))

final_callback = unreal.register_slate_post_tick_callback(finalize)
