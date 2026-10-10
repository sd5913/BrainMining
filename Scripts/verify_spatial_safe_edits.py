import json
import hashlib
from pathlib import Path

import unreal


map_path = "/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo"
disk_map = Path(unreal.Paths.project_content_dir()) / "MiningPrototype/Maps/Lvl_MiningSpatial_Demo.umap"
before_hash = hashlib.sha256(disk_map.read_bytes()).hexdigest()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(map_path)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)[0]
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
main = region.main_tunnel
original = [knot.export_text() for knot in main.knots]
original_positions = [(knot.position.x, knot.position.y, knot.position.z) for knot in main.knots]
original_profiles = [[point.export_text() for point in knot.profile] for knot in main.knots]
report = {"initial_applied": unreal.MiningDesignerLibrary.check_spatial_edit(region)}
report["initial_status"] = unreal.MiningDesignerLibrary.spatial_editing_status()
report["original_preserved_on_check"] = original == [knot.export_text() for knot in main.knots]
report["repair_succeeded"] = unreal.MiningDesignerLibrary.smooth_spatial_issue()
report["repair_status"] = unreal.MiningDesignerLibrary.spatial_editing_status()
report["layout_applied"] = generator.layout_applied
report["build_ms"] = generator.last_build_milliseconds
report["positions_unchanged"] = original_positions == [(knot.position.x, knot.position.y, knot.position.z) for knot in main.knots]
report["profiles_unchanged"] = original_profiles == [[point.export_text() for point in knot.profile] for knot in main.knots]
report["repaired_knots"] = [knot.export_text() for knot in main.knots]
report["map_file_unchanged"] = before_hash == hashlib.sha256(disk_map.read_bytes()).hexdigest()
assert report["original_preserved_on_check"] and report["map_file_unchanged"] and report["positions_unchanged"] and report["profiles_unchanged"]
output = Path(unreal.Paths.project_saved_dir()) / "Validation/spatial_safe_legacy.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("SPATIAL_SAFE_LEGACY " + json.dumps({key: value for key, value in report.items() if key != "repaired_knots"}, ensure_ascii=False))
