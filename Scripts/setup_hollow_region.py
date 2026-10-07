import json
from pathlib import Path

import unreal


MAP = "/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo"
ROOT = "/Game/MiningPrototype/Materials/HollowRegion"
assets = unreal.AssetToolsHelpers.get_asset_tools()
editing = unreal.MaterialEditingLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def material():
    path = ROOT + "/M_HollowRegionRock"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    surface = assets.create_asset("M_HollowRegionRock", ROOT, unreal.Material, unreal.MaterialFactoryNew())

    def expression(kind, horizontal, vertical, **properties):
        node = editing.create_material_expression(surface, kind, horizontal, vertical)
        for key, value in properties.items():
            node.set_editor_property(key, value)
        return node

    mask = expression(unreal.MaterialExpressionCustom, -200, 0, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="Polygonal guide volumes, including freshly excavated surfaces")
    names = ["World", "Count"] + [name for index in range(16) for name in (f"Start{index}", f"End{index}")]
    inputs = []
    for name in names:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    mask.set_editor_property("inputs", inputs)
    code = "float inside = 0;\n"
    for index in range(16):
        code += f"""
if (Count > {index} && World.x >= Start{index}.x - 0.1 && World.x <= End{index}.x + 0.1 && End{index}.x > Start{index}.x)
{{
    float alpha = saturate((World.x - Start{index}.x) / (End{index}.x - Start{index}.x));
    float2 delta = World.yz - lerp(Start{index}.yz, End{index}.yz, alpha);
    float angle = atan2(delta.y, delta.x);
    float normalAngle = (floor(angle / 0.3926990817) + 0.5) * 0.3926990817;
    float distance = dot(delta, float2(cos(normalAngle), sin(normalAngle)));
    inside = max(inside, step(distance, Start{index}.w * 0.9807852804 + 0.1));
}}
"""
    mask.set_editor_property("code", code + "return inside;")
    position = expression(unreal.MaterialExpressionWorldPosition, -1400, -200)
    count = expression(unreal.MaterialExpressionScalarParameter, -1400, 0, parameter_name="GuideCount", default_value=0.0)
    assert editing.connect_material_expressions(position, "", mask, "World")
    assert editing.connect_material_expressions(count, "", mask, "Count")
    for index in range(16):
        for prefix in ("Start", "End"):
            parameter = expression(unreal.MaterialExpressionVectorParameter, -1100, 150 + index * 120, parameter_name=f"Guide{prefix}{index}", default_value=unreal.LinearColor(0, 0, 0, 350))
            assert editing.connect_material_expressions(parameter, "RGBA", mask, f"{prefix}{index}")
    ordinary = expression(unreal.MaterialExpressionVectorParameter, -600, -500, parameter_name="OrdinaryColor", default_value=unreal.LinearColor(0.34, 0.19, 0.075, 1))
    guide = expression(unreal.MaterialExpressionVectorParameter, -600, -350, parameter_name="GuideColor", default_value=unreal.LinearColor(0.40, 0.08, 0.70, 1))
    blend = expression(unreal.MaterialExpressionLinearInterpolate, 0, -200)
    assert editing.connect_material_expressions(ordinary, "", blend, "A")
    assert editing.connect_material_expressions(guide, "", blend, "B")
    assert editing.connect_material_expressions(mask, "", blend, "Alpha")
    scale = expression(unreal.MaterialExpressionScalarParameter, -1000, -700, parameter_name="PatternScale", default_value=0.018)
    scaled = expression(unreal.MaterialExpressionMultiply, -750, -700)
    noise = expression(unreal.MaterialExpressionNoise, -450, -700, scale=1.0, levels=2, quality=1, output_min=0.6, output_max=1.0)
    colored = expression(unreal.MaterialExpressionMultiply, 220, -200)
    glow = expression(unreal.MaterialExpressionScalarParameter, 220, 160, parameter_name="Glow", default_value=0.35)
    emission = expression(unreal.MaterialExpressionMultiply, 440, 0)
    roughness = expression(unreal.MaterialExpressionScalarParameter, 440, 160, parameter_name="Roughness", default_value=0.88)
    for source, target, socket in ((position, scaled, "A"), (scale, scaled, "B"), (scaled, noise, editing.get_material_expression_input_names(noise)[0]), (blend, colored, "A"), (noise, colored, "B"), (colored, emission, "A"), (glow, emission, "B")):
        assert editing.connect_material_expressions(source, "", target, socket)
    assert editing.connect_material_property(colored, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert editing.connect_material_property(emission, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert editing.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    editing.recompile_material(surface)
    assert unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


def create():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        unreal.log("Existing hollow demo preserved; no authored edits overwritten")
        return
    surface = material()
    instance_path = ROOT + "/MI_HollowRegionRock"
    if unreal.EditorAssetLibrary.does_asset_exist(instance_path):
        surface = unreal.load_asset(instance_path)
    else:
        instance = assets.create_asset("MI_HollowRegionRock", ROOT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        editing.set_material_instance_parent(instance, surface)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance)
        surface = instance
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MiningHollowRegion)
    blueprint_path = "/Game/MiningPrototype/Blueprints/BP_MiningHollowRegion"
    if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
        blueprint = assets.create_asset("BP_MiningHollowRegion", "/Game/MiningPrototype/Blueprints", unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
    assert levels.new_level(MAP)
    world = editor.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.MiningPrototypeGameMode)
    world.get_world_settings().set_editor_property("kill_z", -100000)
    generator = actors.spawn_actor_from_class(unreal.MiningTunnelGenerator, unreal.Vector())
    generator.set_actor_label("HOLLOW_GENERATOR_Preview_and_Build")
    generator.set_folder_path("HollowDemo/EDIT")
    generator.set_editor_property("auto_rebuild", False)
    generator.set_editor_property("hollow_region_prototype", True)
    generator.set_editor_property("automatic_chunks", True)
    generator.set_editor_property("chunk_length", 1600)
    generator.set_editor_property("segment_triangle_budget", 30000)
    generator.set_editor_property("tunnel_material", surface)
    generator.set_editor_property("bedrock_material", unreal.load_asset("/Game/MiningPrototype/Materials/DirectRooms/MI_Bedrock_Cool"))
    scale = generator.get_editor_property("player_scale_settings")
    scale.set_editor_property("enabled", True)
    scale.set_editor_property("scale", 0.7)
    generator.set_editor_property("player_scale_settings", scale)
    map_settings = generator.get_editor_property("developer_map_settings")
    map_settings.set_editor_property("show_on_start", True)
    generator.set_editor_property("developer_map_settings", map_settings)
    rooms = []
    for order, location in enumerate(((0, 0, 0), (7200, 0, -800))):
        room = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(*location) + unreal.Vector(0, 0, 500))
        room.set_actor_label("ROOM_A_START_MOVE_AND_RESIZE" if order == 0 else "ROOM_B_DESTINATION_MOVE_AND_RESIZE")
        room.set_folder_path("HollowDemo/EDIT")
        room.set_editor_property("connection_order", order)
        if order == 0:
            room.set_as_start_room()
        room.set_editor_property("room_size", unreal.Vector(1200, 1200, 1000))
        rooms.append(room)
    region_class = unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path)
    region = actors.spawn_actor_from_class(region_class, unreal.Vector())
    region.set_actor_label("REGION_EDIT_Path_Radius_Plugs_RockBounds")
    region.set_folder_path("HollowDemo/EDIT")
    region.set_editor_property("from_room", rooms[0])
    region.set_editor_property("to_room", rooms[1])
    region.fit_path_between_rooms()
    for index, offset in ((1, 150), (3, -150)):
        point = region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD)
        point.y = offset
        region.path.set_location_at_spline_point(index, point, unreal.SplineCoordinateSpace.WORLD, True)
    generator.set_editor_property("hollow_region", region)
    assert generator.rebuild_tunnels(), generator.last_build_status
    generator.show_section()
    generator.set_editor_property("section_keep_positive", False)
    generator.set_editor_property("auto_rebuild", True)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(3000, 2000, 5000), unreal.Rotator(pitch=-50, yaw=-135, roll=0))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(2.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(3000, 0, 3000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_intensity(0.6)
    atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
    exposure.set_editor_property("unbound", True)
    settings = exposure.get_editor_property("settings")
    for name in ("min_brightness", "max_brightness"):
        settings.set_editor_property("override_auto_exposure_" + name, True)
        settings.set_editor_property("auto_exposure_" + name, 3.0)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 0.0)
    exposure.set_editor_property("settings", settings)
    for actor in (sun, sky, atmosphere, exposure):
        actor.set_folder_path("HollowDemo/Lighting")
    for room in rooms:
        light = actors.spawn_actor_from_class(unreal.PointLight, room.get_actor_location() + unreal.Vector(0, 0, 600))
        light.set_actor_label("Room_Light_EDIT_OPTIONAL")
        light.set_folder_path("HollowDemo/Lighting")
        light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.light_component.set_editor_property("attenuation_radius", 1300)
        light.light_component.set_intensity(600)
        light.light_component.set_light_color(unreal.LinearColor(0.45, 0.7, 1))
    editor.set_level_viewport_camera_info(unreal.Vector(3600, 6500, 1000), unreal.Rotator(pitch=-10, yaw=-90, roll=0))
    actors.set_selected_level_actors([region])
    assert levels.save_current_level()
    report = {"passed": True, "map": MAP, "status": generator.last_build_status, "chunks": len(generator.generated_fill.chunks), "triangles": generator.generated_fill.triangle_count, "largest_chunk": generator.generated_fill.largest_chunk_triangles, "guide_volumes": len(generator.generated_fill.guide_material_volumes), "rooms": len(rooms), "plugs": len(region.plugs)}
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_region_install.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_REGION_INSTALL " + json.dumps(report))


create()
