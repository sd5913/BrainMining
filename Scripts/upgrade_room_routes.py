import json
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level(MAP_PATH)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)


def room_snapshot(room):
    position = room.get_actor_location()
    size = room.room_size * room.get_actor_scale3d()
    floor = position.z if room.get_editor_property("bFloorAnchored") else position.z - size.z / 2
    return {"order": room.connection_order, "floor": [position.x, position.y, floor], "size": [size.x, size.y, size.z]}


before = [room_snapshot(room) for room in rooms]
report = {"passed": False, "before": before}
assets = unreal.AssetToolsHelpers.get_asset_tools()
material_path = "/Game/MiningPrototype/Materials/AutoRoutes"


def preview_material(name, color):
    path = material_path + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    surface = assets.create_asset(name, material_path, unreal.Material, unreal.MaterialFactoryNew())
    surface.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tint = unreal.MaterialEditingLibrary.create_material_expression(surface, unreal.MaterialExpressionConstant3Vector)
    tint.set_editor_property("constant", unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(surface)
    unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


generator.set_editor_property("preview_bedrock_material", preview_material("M_Preview_Bedrock", (1.2, 2.2, 3.4)))
generator.set_editor_property("preview_fill_material", preview_material("M_Preview_Diggable", (3.2, 1.5, 0.35)))
result = generator.rebuild_tunnels()
after = [room_snapshot(room) for room in rooms]
report.update({"after": after, "built": result, "status": generator.last_build_status, "warnings": generator.layout_warnings})
report["preserved_room_spaces"] = all(all(abs(first - second) < 0.1 for key in ("floor", "size") for first, second in zip(old[key], new[key])) for old, new in zip(before, after))
report["routes"] = [{"status": route.route_status, "slope": route.maximum_slope, "length": route.route_length} for route in generator.routes]
report["passed"] = result and report["preserved_room_spaces"]
if report["passed"]:
    generator.set_editor_property("auto_rebuild", True)
    generator.show_layout()
    assert levels.save_current_level()
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "route_upgrade.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("ROUTE_UPGRADE " + json.dumps(report))
assert report["passed"], report
