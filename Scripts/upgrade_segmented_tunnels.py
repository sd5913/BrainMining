import json
import re
import traceback
from pathlib import Path
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
report = {"passed": False}

def snapshot():
    def stable(value):
        return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))
    return {"rooms": [{"order": room.connection_order, "transform": stable(room.get_actor_transform()), "size": stable(room.room_size)} for room in rooms], "materials": [stable(generator.bedrock_material), stable(generator.tunnel_material)], "visibility": [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity], "faces": [[str(route.start_wall), str(route.end_wall)] for route in generator.routes], "preview": str(generator.preview_mode)}

old_auto = generator.auto_rebuild
try:
    report["before"] = snapshot()
    generator.set_editor_property("auto_rebuild", False)
    assert generator.rebuild_tunnels(), generator.last_build_status
    assert len(generator.generated_segments) == len(rooms) - 1
    assert len(set(segment.get_name() for segment in generator.generated_segments)) == len(rooms) - 1
    for segment in generator.generated_segments:
        assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(segment.rock_mesh.get_dynamic_mesh())
        assert segment.max_triangles == generator.segment_triangle_budget
    report["after"] = snapshot()
    assert report["before"] == report["after"], "Migration changed room layout or existing user settings"
    report["segments"] = [{"name": segment.get_name(), "index": segment.segment_index, "triangles": segment.triangle_count, "budget": segment.max_triangles} for segment in generator.generated_segments]
    generator.set_editor_property("auto_rebuild", old_auto)
    assert levels.save_current_level()
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "segmented_upgrade.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SEGMENTED_UPGRADE " + json.dumps(report))
