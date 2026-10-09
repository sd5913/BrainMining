import json
from pathlib import Path

import unreal


MAP = "/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo"
BASE = "/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), "Refusing to overwrite an existing chain demo"
assert levels.new_level_from_template(MAP, BASE)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
generator.set_actor_label("多矿区生成器_矿业策划台可编辑")
original_region = generator.hollow_region
original_points = [original_region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(original_region.path.get_number_of_spline_points())]
for index in (2, 3):
    room = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(index * 7200, 0, index * -800 + 500))
    room.set_actor_label("ROOM_%02d_移动地板中心_调整尺寸" % index)
    room.set_folder_path("HollowChain/EDIT/Rooms")
    room.set_editor_property("connection_order", index)
    room.set_editor_property("room_size", unreal.Vector(1200, 1200, 1000))
assert generator.add_missing_hollow_regions(), generator.last_build_status
assert list(generator.hollow_regions)[0] == original_region
assert original_points == [original_region.path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD) for index in range(original_region.path.get_number_of_spline_points())]
chamber_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/MiningPrototype/Blueprints/BP_MiningHollowChamber")
for index, region in enumerate(generator.hollow_regions):
    region.set_actor_label("REGION_%02d_TO_%02d_路径和尺寸可编辑" % (index, index + 1))
    region.set_folder_path("HollowChain/EDIT/Regions")
    region.set_editor_property("region_width", 4600)
    if index:
        region.set_editor_property("passage_radius", 300 if index == 1 else 400)
        region.fit_path_between_rooms()
    assert generator.rebuild_tunnels(), generator.last_build_status
    fraction = 0.14 if index != 2 else 0.60
    anchor = region.path.get_location_at_distance_along_spline(region.path.get_spline_length() * fraction, unreal.SplineCoordinateSpace.WORLD)
    floor = anchor + unreal.Vector(0, 1500 if index != 1 else -1500, -region.passage_radius)
    chamber = actors.spawn_actor_from_class(chamber_class, floor)
    chamber.set_actor_label("CAVE_%02d_%s" % (index, "HIDDEN" if index == 1 else "OPEN"))
    chamber.set_folder_path("HollowChain/EDIT/Chambers")
    chamber.set_editor_property("region", region)
    chamber.set_editor_property("connection_position", fraction)
    chamber.set_editor_property("hidden_entrance", index == 1)
    chamber.set_editor_property("chamber_size", unreal.Vector(900, 900, 700))
generator.set_editor_property("completed_segment_policy", unreal.CompletedMiningSegments.SEAL_AND_RECYCLE)
assert generator.rebuild_tunnels(), generator.last_build_status
assert len(generator.generated_segments) == 3 and generator.space_count == 3
generator.set_editor_property("section_axis", 1)
generator.set_editor_property("section_position", 0)
generator.set_editor_property("section_keep_positive", False)
generator.show_section()
generator.set_editor_property("auto_rebuild", True)
editor.set_level_viewport_camera_info(unreal.Vector(10800, 21500, 10000), unreal.Rotator(pitch=-28, yaw=-90, roll=0))
actors.set_selected_level_actors([generator])
assert levels.save_current_level()
report = {"passed": True, "map": MAP, "status": generator.last_build_status, "chunks": [len(segment.chunks) for segment in generator.generated_segments], "largest_chunks": [segment.largest_chunk_triangles for segment in generator.generated_segments]}
(Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_chain_install.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("HOLLOW_CHAIN_INSTALL " + json.dumps(report))
