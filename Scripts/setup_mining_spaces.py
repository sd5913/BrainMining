import json
from pathlib import Path

import unreal


assets = unreal.AssetToolsHelpers.get_asset_tools()
editing = unreal.MaterialEditingLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
preview_path = "/Game/MiningPrototype/Materials/SpaceEditing/M_SpacePreview"
if unreal.EditorAssetLibrary.does_asset_exist(preview_path):
    material = unreal.load_asset(preview_path)
else:
    material = assets.create_asset("M_SpacePreview", "/Game/MiningPrototype/Materials/SpaceEditing", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("disable_depth_test", True)
    color = editing.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.12, 0.7, 1, 1))
    opacity = editing.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 180)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.16)
    assert editing.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert editing.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    editing.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)

blueprint_path = "/Game/MiningPrototype/Blueprints/BP_MiningLayoutSpace"
if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MiningLayoutSpace)
    blueprint = assets.create_asset("BP_MiningLayoutSpace", "/Game/MiningPrototype/Blueprints", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("preview_material", material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

demo_path = "/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo"
created = not unreal.EditorAssetLibrary.does_asset_exist(demo_path)
if created:
    assert unreal.EditorAssetLibrary.duplicate_asset("/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms", demo_path)
    assert levels.load_level(demo_path)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
    generator.set_editor_property("auto_rebuild", False)
    rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
    positions = ((0, 0, 0), (9000, 0, 0), (18000, 0, -2000), (26000, 0, -4000))
    for room, position in zip(rooms, positions):
        room.set_actor_scale3d(unreal.Vector(1, 1, 1))
        room.set_actor_location(unreal.Vector(*position), False, True)
        room.set_editor_property("room_size", unreal.Vector(1000, 1400, 1200))
    spaces = []
    for name, position, size, shape, contents, connection, entrance in (
        ("SPACE_01_DiggableExpansion", (2400, 0, 0), (1200, 2300, 1300), unreal.MiningSpaceShape.BOX, unreal.MiningSpaceContents.DIGGABLE_ROCK, unreal.MiningSpaceConnection.OVERLAP, unreal.MiningSpaceEntrance.ROCK_FILLED),
        ("SPACE_02_HiddenCave", (4500, 2400, 0), (1400, 1200, 1000), unreal.MiningSpaceShape.BOX, unreal.MiningSpaceContents.EMPTY, unreal.MiningSpaceConnection.STRAIGHT_BRANCH, unreal.MiningSpaceEntrance.HIDDEN_WALL),
        ("SPACE_03_OpenEllipsoid", (6800, -1900, -100), (1600, 1400, 1200), unreal.MiningSpaceShape.ELLIPSOID, unreal.MiningSpaceContents.EMPTY, unreal.MiningSpaceConnection.STRAIGHT_BRANCH, unreal.MiningSpaceEntrance.OPEN),
    ):
        space = actors.spawn_actor_from_class(unreal.MiningLayoutSpace, unreal.Vector(*position))
        space.set_actor_label(name)
        space.set_folder_path("AutoMine/Main/Spaces_EDIT")
        space.set_editor_property("space_name", name.split("_", 2)[2])
        space.set_editor_property("from_room", rooms[0])
        space.set_editor_property("space_size", unreal.Vector(*size))
        space.set_editor_property("shape", shape)
        space.set_editor_property("contents", contents)
        space.set_editor_property("connection", connection)
        space.set_editor_property("entrance", entrance)
        space.set_editor_property("branch_width", 450)
        space.set_editor_property("branch_height", 450)
        space.set_editor_property("preview_material", material)
        spaces.append(space)
        if contents == unreal.MiningSpaceContents.EMPTY:
            light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(position[0], position[1], position[2] + size[2] - 150))
            light.set_actor_label(name + "_ContentLight")
            light.set_folder_path("AutoMine/Main/SpaceContent")
            light.attach_to_component(space.content_root, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
            light.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            light.light_component.set_intensity(400)
            light.light_component.set_attenuation_radius(1600)
    assert generator.rebuild_tunnels(), generator.last_build_status
    assert generator.space_count == 3 and generator.segment_triangle_budget == 30000
    generator.show_layout()
    generator.set_editor_property("auto_rebuild", True)
    actors.set_selected_level_actors([spaces[1]])
    assert levels.save_current_level()
    report = {"passed": True, "demo": demo_path, "spaces": [space.get_path_name() for space in spaces], "chunks": [len(segment.chunks) for segment in generator.generated_segments], "triangles": [segment.triangle_count for segment in generator.generated_segments], "original_map_modified": False}
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "mining_spaces_install.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("MINING_SPACES_INSTALL " + json.dumps(report))
else:
    unreal.log("Mining spaces assets exist; existing demo preserved without overwriting edits")
