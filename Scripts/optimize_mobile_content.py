import json
import re
import shutil
from pathlib import Path

import unreal

root = Path(unreal.Paths.project_dir()).resolve()
report_dir = root / "Saved/Validation/SizeOptimization"
report_dir.mkdir(parents=True, exist_ok=True)
backup_dir = root / "Saved/Backups/SizeOptimization20261021"
backup_dir.mkdir(parents=True, exist_ok=True)
mesh_path = "/Game/Fab/Cell/Cell/StaticMeshes/cell"
source_file = root / "Content/Fab/Cell/Cell/StaticMeshes/cell.uasset"
backup_file = backup_dir / "cell.uasset"
if not backup_file.exists():
    shutil.copy2(source_file, backup_file)
mesh = unreal.load_asset(mesh_path)
assert isinstance(mesh, unreal.StaticMesh), mesh_path
before = {"lod_triangles": [mesh.get_num_triangles(index) for index in range(mesh.get_num_lods())], "platform_lods": {str(key): value for key, value in mesh.get_minimum_lod_for_platforms().items()}}
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
if mesh.get_num_lods() < 2:
    options = unreal.StaticMeshReductionOptions()
    options.set_editor_property("auto_compute_lod_screen_size", False)
    options.set_editor_property("reduction_settings", [
        unreal.StaticMeshReductionSettings(percent_triangles=1.0, screen_size=1.0),
        unreal.StaticMeshReductionSettings(percent_triangles=0.075, screen_size=0.0),
    ])
    assert editor.set_lods(mesh, options) >= 2
platforms = dict(mesh.get_minimum_lod_for_platforms())
platforms["Android"] = 1
mesh.set_minimum_lod_for_platforms(platforms)
assert mesh.get_num_triangles(0) == before["lod_triangles"][0]
assert 1000 <= mesh.get_num_triangles(1) <= 150000
assert mesh.get_minimum_lod_for_platform("Android") == 1
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)

native_paths = set()
for source_root in (root / "Source", root / "Plugins"):
    for source_file in source_root.rglob("*"):
        if source_file.suffix in (".cpp", ".h"):
            source_text = source_file.read_text(encoding="utf-8-sig", errors="replace")
            native_paths.update(path.split(".")[0] for path in re.findall(r"/Game/MainGame/Materials/[A-Za-z0-9_./]+", source_text))
native_paths = sorted(native_paths)
assert native_paths
native_assets = [unreal.load_asset(path) for path in native_paths]
assert all(native_assets), native_paths
label_path = "/Game/Packaging/PAL_NativeRuntimeMaterials"
label = unreal.load_asset(label_path)
if label is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.PrimaryAssetLabel)
    label = unreal.AssetToolsHelpers.get_asset_tools().create_asset("PAL_NativeRuntimeMaterials", "/Game/Packaging", unreal.PrimaryAssetLabel, factory)
assert isinstance(label, unreal.PrimaryAssetLabel)
label.set_editor_property("explicit_assets", native_assets)
label.set_editor_property("is_runtime_label", False)
rules = unreal.PrimaryAssetRules()
rules.set_editor_property("priority", 1)
rules.set_editor_property("chunk_id", -1)
rules.set_editor_property("apply_recursively", True)
rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
label.set_editor_property("rules", rules)
assert unreal.EditorAssetLibrary.save_loaded_asset(label, False)
after = {"lod_triangles": [mesh.get_num_triangles(index) for index in range(mesh.get_num_lods())], "platform_lods": {str(key): value for key, value in mesh.get_minimum_lod_for_platforms().items()}}
result = {"passed": True, "mesh": mesh_path, "backup": str(backup_file), "before": before, "after": after, "native_label": label_path, "native_materials": native_paths}
(report_dir / "content_changes.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MOBILE_SIZE_OPTIMIZATION " + json.dumps(result))
