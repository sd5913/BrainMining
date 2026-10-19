"""Read-only verification of the P2 rabbit and HeavenToilet water material."""
import json
import pathlib
import unreal

out = pathlib.Path(unreal.Paths.project_saved_dir()) / "Automation"
out.mkdir(parents=True, exist_ok=True)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

P2 = "/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2"
assert levels.load_level(P2), P2
stage = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.CardRoomStage)), None)
assert stage
rabbit = stage.get_editor_property("rabbit_skeletal_mesh")
rabbit_component = next((c for c in stage.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == "RabbitOpponent"), None)
seated = next((c for c in stage.get_components_by_class(unreal.PoseableMeshComponent) if c.get_name() == "RabbitSeatedOpponent"), None)
rabbit_report = {
    "map": P2,
    "stage": stage.get_path_name(),
    "mesh": rabbit.get_path_name() if rabbit else "",
    "rabbit_visual_scale": stage.get_editor_property("rabbit_visual_scale"),
    "rabbit_landing_local": str(stage.get_editor_property("rabbit_landing_local")),
    "physics_drop": stage.get_editor_property("rabbit_physics_drop"),
    "physics_asset": rabbit.get_editor_property("physics_asset").get_path_name() if rabbit and rabbit.get_editor_property("physics_asset") else "",
    "runtime_component_mesh": rabbit_component.skeletal_mesh.get_path_name() if rabbit_component and rabbit_component.skeletal_mesh else "",
    "seated_component_present": seated is not None,
}

HEAVEN = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
assert levels.load_level(HEAVEN), HEAVEN
water = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.HeavenToiletWater)), None)
assert water
basin = water.get_editor_property("water_material")
spill = water.get_editor_property("spill_material")
basin_parent = basin.get_editor_property("parent") if basin and isinstance(basin, unreal.MaterialInstance) else None
spill_parent = spill.get_editor_property("parent") if spill and isinstance(spill, unreal.MaterialInstance) else None
water_report = {
    "map": HEAVEN,
    "water_actor": water.get_path_name(),
    "water_material": basin.get_path_name() if basin else "",
    "water_material_parent": basin_parent.get_path_name() if basin_parent else "",
    "spill_material": spill.get_path_name() if spill else "",
    "spill_material_parent": spill_parent.get_path_name() if spill_parent else "",
    "custom_water_master": unreal.EditorAssetLibrary.does_asset_exist("/Game/HeavenPreview/Materials/M_HeavenClearWater"),
    "basin_component_material": water.basin.get_material(0).get_path_name() if water.basin and water.basin.get_material(0) else "",
    "spill_component_material": water.spill.get_material(0).get_path_name() if water.spill and water.spill.get_material(0) else "",
}
report = {"rabbit": rabbit_report, "water": water_report}
(out / "rabbit_water_state.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("RABBIT_WATER_STATE", json.dumps(report, ensure_ascii=False))
