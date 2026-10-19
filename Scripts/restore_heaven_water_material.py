"""Rebind HeavenToilet's authored surface to the custom HLSL water instances."""
import datetime
import json
import pathlib
import shutil
import unreal

MAP = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
BASIN = "/Game/HeavenPreview/Materials/MI_HeavenClearBasin"
SPILL = "/Game/HeavenPreview/Materials/MI_HeavenClearOutflow"
MASTER = "/Game/HeavenPreview/Materials/M_HeavenClearWater"
root = pathlib.Path(unreal.Paths.project_dir()).resolve()
map_file = root / "Content" / "CollectionPrototype" / "HeavenToilet" / "Maps" / "Lvl_HeavenToilet.umap"
backup = root / "Saved" / "HeavenPreview" / ("BeforeWaterMaterialRebind_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
backup.mkdir(parents=True, exist_ok=True)
shutil.copy2(map_file, backup / map_file.name)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP), MAP
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
water = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.HeavenToiletWater)), None)
assert water, "HeavenToiletWater actor missing"
basin = unreal.load_asset(BASIN)
spill = unreal.load_asset(SPILL)
master = unreal.load_asset(MASTER)
assert basin and spill and master

# Keep all authored transforms, visibility and breach parameters intact. Only
# replace the two material references that control the water surface visuals.
water.set_editor_property("water_material", basin)
water.set_editor_property("spill_material", spill)
water.basin.set_material(0, basin)
water.spill.set_material(0, spill)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

report = {
    "map": MAP,
    "water_material": basin.get_path_name(),
    "spill_material": spill.get_path_name(),
    "custom_hlsl_master": master.get_path_name(),
    "backup": str(backup),
}
out = root / "Saved" / "HeavenPreview"
(out / "water_material_rebind.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("HEAVEN_WATER_MATERIAL_REBOUND", json.dumps(report, ensure_ascii=False))
