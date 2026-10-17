"""Load and save the restored HeavenToilet map without changing authored transforms."""
import json
import pathlib
import unreal

MAP = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert LEVELS.load_level(MAP), MAP
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world and world.get_path_name().startswith(MAP + "."), world
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
report = {
    "map": MAP,
    "saved": True,
    "duck": unreal.EditorAssetLibrary.does_asset_exist("/Game/HeavenPreview/Meshes/SM_DarkDuck"),
}
root = pathlib.Path(unreal.Paths.project_saved_dir()) / "HeavenPreview"
root.mkdir(parents=True, exist_ok=True)
(root / "restore_save.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("HEAVEN_TOILET_RESTORED_AND_SAVED", json.dumps(report, ensure_ascii=False))
