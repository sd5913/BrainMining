import json
import pathlib
import unreal

MAP = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP), MAP
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
toilets = [a for a in actors if isinstance(a, unreal.HeavenToilet)]
water = [a for a in actors if isinstance(a, unreal.HeavenToiletWater)]
report = {
    "map": world.get_path_name() if world else None,
    "toilet_actors": len(toilets),
    "water_actors": len(water),
    "duck_asset": unreal.EditorAssetLibrary.does_asset_exist("/Game/HeavenPreview/Meshes/SM_DarkDuck"),
    "hall_sky": unreal.EditorAssetLibrary.does_asset_exist("/Game/HeavenPreview/Materials/M_HeavenHallSky"),
    "hall_pipe": unreal.EditorAssetLibrary.does_asset_exist("/Game/HeavenPreview/Meshes/Pipes/SM_HeavenPipePart_002"),
}
root = pathlib.Path(unreal.Paths.project_saved_dir()) / "HeavenPreview"
root.mkdir(parents=True, exist_ok=True)
(root / "restore_verify.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
assert report["duck_asset"] and report["hall_sky"] and report["hall_pipe"]
print("HEAVEN_RESTORE_VERIFY", json.dumps(report, ensure_ascii=False))
