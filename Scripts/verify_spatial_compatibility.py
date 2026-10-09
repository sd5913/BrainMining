import hashlib
import json
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
directory = Path(unreal.Paths.project_content_dir()) / "MiningPrototype" / "Maps"
files = sorted(path for path in directory.glob("*.umap") if "Spatial" not in path.name)
before = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
report = {"passed": False, "maps": []}
try:
    for path in files:
        asset = "/Game/MiningPrototype/Maps/" + path.stem
        assert levels.load_level(asset), asset
        world = editor.get_editor_world()
        entry = {"map": asset, "generators": []}
        for generator in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator):
            generator.set_editor_property("auto_rebuild", False)
            assert generator.rebuild_tunnels(), asset + ": " + generator.last_build_status
            assert generator.segment_triangle_budget == 30000
            assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh())
            for segment in generator.generated_segments:
                assert segment.max_triangles == 30000 and len(segment.chunks) <= 128
                assert all(unreal.GeometryScript_MeshQueries.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks)
            entry["generators"].append(generator.last_build_status)
        report["maps"].append(entry)
    assert before == {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files}
    report.update({"passed": True, "old_maps_checked": len(files), "old_maps_changed": 0})
except Exception:
    report["error"] = traceback.format_exc()
finally:
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "spatial_compatibility.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_COMPATIBILITY " + json.dumps(report))
