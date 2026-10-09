import hashlib
import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
map_directory = Path(unreal.Paths.project_content_dir()) / "MiningPrototype" / "Maps"
files = sorted(path for path in map_directory.glob("*") if path.is_file() and "HollowChain" not in path.name)
before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
report = {"passed": False, "maps": []}
try:
    for path in files:
        if path.suffix != ".umap":
            continue
        asset_path = "/Game/MiningPrototype/Maps/" + path.stem
        assert levels.load_level(asset_path), asset_path
        world = editor.get_editor_world()
        generators = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)
        entry = {"map": asset_path, "generators": []}
        for generator in generators:
            generator.set_editor_property("auto_rebuild", False)
            assert generator.rebuild_tunnels(), asset_path + ": " + generator.last_build_status
            assert generator.segment_triangle_budget == 30000
            assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh())
            if generator.hollow_region_prototype:
                assert not generator.hollow_regions and len(generator.get_hollow_regions()) == 1
                assert len(generator.generated_segments) == 1
                assert all(item.group_index == 0 for item in generator.generated_map_regions if item.kind != unreal.MiningMapRegionKind.ROOM)
                rock = generator.generated_fill
                guide = rock.guide_material_volumes[0]
                assert rock.mine_sphere((guide.start + guide.end) * 0.5, 180)
                rock.reset_rock()
                assert rock.successful_cuts == 0
            entry["generators"].append({"rooms": generator.room_count, "segments": len(generator.generated_segments), "hollow_legacy": generator.hollow_region_prototype, "status": generator.last_build_status})
        report["maps"].append(entry)
    after = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
    assert before == after, "Compatibility checks must never save legacy maps"
    report.update({"passed": True, "old_files_checked": len(files), "old_files_changed": 0})
except Exception:
    report["error"] = traceback.format_exc()
finally:
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_chain_compatibility.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_CHAIN_COMPATIBILITY " + json.dumps(report))
