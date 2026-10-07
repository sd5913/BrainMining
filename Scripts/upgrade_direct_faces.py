import json
import traceback
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms"
MATERIAL_PATH = "/Game/MiningPrototype/Materials/DirectRooms"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP_PATH)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
assets = unreal.AssetToolsHelpers.get_asset_tools()
materials = unreal.MaterialEditingLibrary
report = {"passed": False}


def vector(value):
    return [value.x, value.y, value.z]


def snapshot():
    return [{"name": room.get_actor_label(), "order": room.connection_order, "position": vector(room.get_actor_location()), "size": vector(room.room_size), "scale": vector(room.get_actor_scale3d()), "rotation": [room.get_actor_rotation().pitch, room.get_actor_rotation().yaw, room.get_actor_rotation().roll]} for room in rooms]


def master_material():
    name = "M_ReadableRock"
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PATH + "/" + name):
        return unreal.load_asset(MATERIAL_PATH + "/" + name)
    surface = assets.create_asset(name, MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())

    def expression(kind, horizontal, vertical, **properties):
        node = materials.create_material_expression(surface, kind, horizontal, vertical)
        for key, value in properties.items():
            node.set_editor_property(key, value)
        return node

    tint = expression(unreal.MaterialExpressionVectorParameter, -800, -200, parameter_name="BaseColor", default_value=unreal.LinearColor(0.3, 0.18, 0.07, 1))
    scale = expression(unreal.MaterialExpressionScalarParameter, -1200, 200, parameter_name="PatternScale", default_value=0.018)
    position = expression(unreal.MaterialExpressionWorldPosition, -1200, 0)
    scaled = expression(unreal.MaterialExpressionMultiply, -1000, 0)
    noise = expression(unreal.MaterialExpressionNoise, -800, 100, scale=1.0, levels=2, quality=1, output_min=0.55, output_max=1.0)
    colored = expression(unreal.MaterialExpressionMultiply, -500, -100)
    glow = expression(unreal.MaterialExpressionScalarParameter, -500, 300, parameter_name="Glow", default_value=0.35)
    emission = expression(unreal.MaterialExpressionMultiply, -200, 200)
    roughness = expression(unreal.MaterialExpressionScalarParameter, -200, 400, parameter_name="Roughness", default_value=0.88)
    noise_input = materials.get_material_expression_input_names(noise)[0]
    for source, target, socket in ((position, scaled, "A"), (scale, scaled, "B"), (scaled, noise, noise_input), (tint, colored, "A"), (noise, colored, "B"), (colored, emission, "A"), (glow, emission, "B")):
        assert materials.connect_material_expressions(source, "", target, socket), str(target) + " input " + socket
    assert materials.connect_material_property(colored, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert materials.connect_material_property(emission, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert materials.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    materials.recompile_material(surface)
    unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


def instance(name, color, parent):
    path = MATERIAL_PATH + "/" + name
    surface = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name, MATERIAL_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    materials.set_material_instance_parent(surface, parent)
    materials.set_material_instance_vector_parameter_value(surface, "BaseColor", unreal.LinearColor(*color, 1))
    materials.set_material_instance_scalar_parameter_value(surface, "Roughness", 0.88)
    materials.set_material_instance_scalar_parameter_value(surface, "Glow", 0.35)
    materials.set_material_instance_scalar_parameter_value(surface, "PatternScale", 0.018)
    unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


try:
    report["before"] = snapshot()
    generator.set_editor_property("auto_rebuild", False)
    generator.set_editor_property("direct_room_faces", True)
    generator.set_editor_property("improve_mining_visibility", True)
    generator.set_editor_property("headlamp_intensity", 4)
    generator.set_editor_property("fill_light_intensity", 1)
    for route in generator.routes:
        route.set_editor_property("start_wall", unreal.MiningDoorWall.AUTO)
        route.set_editor_property("end_wall", unreal.MiningDoorWall.AUTO)
    parent = master_material()
    generator.set_editor_property("bedrock_material", instance("MI_Bedrock_Cool", (0.10, 0.16, 0.23), parent))
    generator.set_editor_property("tunnel_material", instance("MI_Diggable_Warm", (0.34, 0.19, 0.075), parent))
    assert generator.rebuild_tunnels(), generator.last_build_status
    report["after"] = snapshot()
    assert report["before"] == report["after"], "Room layout changed during migration"
    query = unreal.GeometryScript_MeshQueries
    assert query.get_is_closed_mesh(generator.generated_bedrock.dynamic_mesh_component.get_dynamic_mesh())
    assert query.get_is_closed_mesh(generator.generated_fill.rock_mesh.get_dynamic_mesh())
    report["routes"] = [{"from": route.from_room.connection_order, "to": route.to_room.connection_order, "slope": route.maximum_slope, "length": route.route_length, "start_wall": route.get_editor_property("ResolvedStartWall"), "end_wall": route.get_editor_property("ResolvedEndWall"), "points": [vector(route.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD)) for index in range(route.path.get_number_of_spline_points())]} for route in generator.routes]
    report["postprocess"] = []
    for volume in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PostProcessVolume):
        settings = volume.settings
        report["postprocess"].append({"name": volume.get_actor_label(), "unbound": volume.unbound, "min_ev": settings.auto_exposure_min_brightness, "max_ev": settings.auto_exposure_max_brightness, "bias": settings.auto_exposure_bias, "override_min": settings.override_auto_exposure_min_brightness, "override_max": settings.override_auto_exposure_max_brightness})
    generator.set_editor_property("auto_rebuild", True)
    generator.show_layout()
    generator.set_actor_label("AUTO_TUNNELS_RoomFaces_Bedrock_Visibility")
    report["status"] = generator.last_build_status
    report["warnings"] = generator.layout_warnings
    report["passed"] = True
    assert levels.save_current_level()
except Exception:
    report["error"] = traceback.format_exc()
    raise
finally:
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "direct_faces_upgrade.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DIRECT_FACES_UPGRADE " + json.dumps(report))
