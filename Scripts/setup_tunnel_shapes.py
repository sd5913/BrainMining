import json
from pathlib import Path

import unreal


assets = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
blueprint_path = "/Game/MiningPrototype/Blueprints/BP_MiningTunnelShapeSection"
if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MiningTunnelShapeSection)
    blueprint = assets.create_asset("BP_MiningTunnelShapeSection", "/Game/MiningPrototype/Blueprints", unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    assert unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

demo = "/Game/MiningPrototype/Maps/Lvl_MiningShapes_Demo"
created = not unreal.EditorAssetLibrary.does_asset_exist(demo)
if created:
    assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
else:
    assert levels.load_level(demo)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
if created:
    generator.set_editor_property("auto_rebuild", False)
    assert generator.rebuild_tunnels(), generator.last_build_status
    route = generator.routes[1 if len(generator.routes) > 1 else 0]
    sections = [route.create_shape_section() for _ in range(3)]
    assert all(sections)
    axis = route.resolved_start_wall // 2
    sections.sort(key=lambda section: (section.get_actor_location().x, section.get_actor_location().y, section.get_actor_location().z)[axis])
    for section, offset, scale in zip(sections, [600, -650, 350], [0.9, 1.35, 0.85]):
        point = section.get_actor_location()
        if axis == 0:
            point.y += offset
        else:
            point.x += offset
        section.set_actor_location(point, False, True)
        section.set_editor_property("width", section.width * scale)
    assert generator.rebuild_tunnels(), generator.last_build_status
    generator.show_layout()
    generator.set_editor_property("auto_rebuild", True)
    actors.set_selected_level_actors([sections[1]])
    focus = sections[1].get_actor_location()
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(focus + unreal.Vector(-2600, -4500, 3500), unreal.Rotator(-34, 60, 0))
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, demo)
    report = {"passed": True, "demo": demo, "sections": [section.get_path_name() for section in sections], "chunks": [len(segment.chunks) for segment in generator.generated_segments], "triangles": [segment.triangle_count for segment in generator.generated_segments], "original_maps_modified": False}
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "tunnel_shapes_install.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("TUNNEL_SHAPES_INSTALL " + json.dumps(report))
else:
    unreal.log("Existing shape demo preserved; no authored edits overwritten")
