import json
from pathlib import Path

import unreal


MAP = "/Game/MiningPrototype/Maps/Lvl_MiningHollowBranches_Demo"
BASE = "/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), "Refusing to overwrite existing branch demo"
assert levels.new_level_from_template(MAP, BASE)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
region = generator.hollow_region
region.set_editor_property("region_width", 4600)
assert generator.rebuild_tunnels(), generator.last_build_status
blueprint_path = "/Game/MiningPrototype/Blueprints/BP_MiningHollowChamber"
if not unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.MiningHollowChamber)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_MiningHollowChamber", "/Game/MiningPrototype/Blueprints", unreal.Blueprint, factory)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
chamber_class = unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path)
for hidden, fraction, side in ((False, 0.14, 1), (True, 0.60, -1)):
    anchor = region.path.get_location_at_distance_along_spline(region.path.get_spline_length() * fraction, unreal.SplineCoordinateSpace.WORLD)
    floor = anchor + unreal.Vector(0, side * 1500, -region.passage_radius)
    chamber = actors.spawn_actor_from_class(chamber_class, floor)
    chamber.set_actor_label("CAVE_HIDDEN_MOVE_RESIZE" if hidden else "CAVE_OPEN_MOVE_RESIZE")
    chamber.set_folder_path("HollowDemo/EDIT/Chambers")
    chamber.set_editor_property("region", region)
    chamber.set_editor_property("connection_position", fraction)
    chamber.set_editor_property("hidden_entrance", hidden)
    chamber.set_editor_property("chamber_size", unreal.Vector(900, 900, 700))
    assert generator.rebuild_tunnels(), generator.last_build_status
generator.set_editor_property("section_axis", 2)
generator.set_editor_property("section_position", 50)
generator.set_editor_property("section_keep_positive", False)
generator.show_section()
generator.set_editor_property("auto_rebuild", True)
editor.set_level_viewport_camera_info(unreal.Vector(3600, 0, 9200), unreal.Rotator(pitch=-90, yaw=-90, roll=0))
actors.set_selected_level_actors([region])
assert levels.save_current_level()
report = {"passed": True, "map": MAP, "status": generator.last_build_status, "chunks": len(generator.generated_fill.chunks), "triangles": generator.generated_fill.triangle_count, "largest_chunk": generator.generated_fill.largest_chunk_triangles}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_branches_install.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("HOLLOW_BRANCHES_INSTALL " + json.dumps(report))
