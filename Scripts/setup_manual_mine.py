import json
from pathlib import Path

import unreal


MAP = "/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo"
BASE = "/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    assert "-ManualSetupRetry" in unreal.SystemLibrary.get_command_line(), "Never overwrite the user's manual map"
    assert levels.load_level(MAP)
    existing_world = editor.get_editor_world()
    assert not unreal.GameplayStatics.get_all_actors_of_class(existing_world, unreal.MiningManualRegion), "Existing manual authoring must never be replaced"
else:
    assert levels.new_level_from_template(MAP, BASE)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
generator.set_actor_label("手绘矿区生成器_只转换手工数据")
generator.set_editor_property("hollow_region", None)
generator.set_editor_property("hollow_regions", [])
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowRegion):
    actors.destroy_actor(actor)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
assert len(rooms) == 2
rooms[0].set_actor_label("主房间_00_START_可移动缩放尺寸")
rooms[1].set_actor_label("主房间_01_终点")
region = actors.spawn_actor_from_class(unreal.MiningManualRegion, unreal.Vector(3600, 0, -100))
region.set_actor_label("手绘矿区_轮廓和空间都可以编辑")
region.set_folder_path("ManualMine/Region")
region.set_editor_property("from_room", rooms[0])
region.set_editor_property("to_room", rooms[1])
region.set_editor_property("rock_size", unreal.Vector(6000, 2400, 3600))
region.set_editor_property("curve_tolerance", 8.0)


def point(horizontal, vertical, tangent=None):
    result = unreal.MiningPenPoint()
    result.set_editor_property("position", unreal.Vector2D(horizontal, vertical))
    if tangent:
        result.set_editor_property("mode", unreal.MiningPenMode.SMOOTH)
        result.set_editor_property("leave", unreal.Vector2D(*tangent))
        result.set_editor_property("arrive", unreal.Vector2D(-tangent[0], -tangent[1]))
    return result


def shape(label, kind, location, size=(1000, 700), depth=700):
    result = actors.spawn_actor_from_class(unreal.MiningManualShape, unreal.Vector(*location))
    result.set_actor_label(label)
    result.set_folder_path("ManualMine/Spaces")
    result.set_editor_property("region", region)
    result.set_editor_property("kind", kind)
    result.set_editor_property("size", unreal.Vector2D(*size))
    result.set_editor_property("depth", depth)
    return result


main = shape("手绘_主通道_两侧分别拉点", unreal.MiningManualKind.PASSAGE, (0, 0, 0))
start = main.start_portal
start.set_editor_property("room", rooms[0])
main.set_editor_property("start_portal", start)
finish = main.end_portal
finish.set_editor_property("room", rooms[1])
main.set_editor_property("end_portal", finish)
main.set_editor_property("boundary_a", [point(600, 750), point(1100, 780, (180, 0)), point(2400, 1100, (400, 0)), point(3500, 150, (240, -100)), point(4650, 450, (300, 0)), point(6100, -50, (200, 0)), point(6600, -50)])
main.set_editor_property("boundary_b", [point(600, 250), point(1100, 250, (180, 0)), point(2250, 200, (350, -100)), point(3450, -650, (220, 0)), point(4750, -200, (280, 0)), point(6100, -550, (200, 0)), point(6600, -550)])
region.set_editor_property("main_passage", main)
hall = shape("空洞_椭圆洞厅_与通道自然合并", unreal.MiningManualKind.CAVITY, (2400, 0, 400), (1300, 1050), 900)
hall.reset_ellipse()
hidden = shape("空洞_隐藏_隔着普通岩层", unreal.MiningManualKind.CAVITY, (1500, 0, -1500), (600, 500), 650)
hidden.set_editor_property("hidden_cavity", True)
hidden.reset_ellipse()
side = shape("支线房间_移动和黄柄调整尺寸", unreal.MiningManualKind.SIDE_ROOM, (4600, 0, -1750), (1100, 650), 1000)
branch = shape("支线_两侧可编辑_终点跟随房间", unreal.MiningManualKind.PASSAGE, (0, 0, 0), depth=650)
finish = branch.end_portal
finish.set_editor_property("room", side)
finish.set_editor_property("wall", unreal.MiningPortalWall.POSITIVE_Z)
finish.set_editor_property("aperture", 400)
branch.set_editor_property("end_portal", finish)
branch.set_editor_property("boundary_a", [point(4000, -250), point(4400, -560, (160, -200)), point(4800, -900), point(4800, -1100)])
branch.set_editor_property("boundary_b", [point(3650, -500), point(3900, -850, (120, -100)), point(4400, -900), point(4400, -1100)])
concave = shape("空洞_凹多边形_不取凸包", unreal.MiningManualKind.CAVITY, (0, 0, 0), depth=600)
concave.set_editor_property("boundary_a", [point(5200, 650), point(6000, 650), point(6000, 1500), point(5200, 1500), point(5200, 1200), point(5650, 1200), point(5650, 950), point(5200, 950)])
block = shape("可挖堵块_闭合轮廓保留岩石", unreal.MiningManualKind.ROCK_BLOCK, (3200, 0, -1000), (240, 1600), 1000)
block.reset_rectangle()
generator.set_editor_property("hollow_regions", [region])
generator.set_editor_property("chunk_length", 1200)
generator.set_editor_property("completed_segment_policy", unreal.CompletedMiningSegments.SEAL_AND_RECYCLE)
for item in sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualShape), key=lambda actor: actor.get_path_name()):
    unreal.log("MANUAL_INPUT " + item.get_path_name() + " " + item.get_actor_label() + " " + str(item.get_actor_location()))
assert generator.rebuild_tunnels(), generator.last_build_status
assert generator.room_count == 2
assert generator.space_count == 6
generator.set_editor_property("section_axis", 1)
generator.set_editor_property("section_position", 0)
generator.set_editor_property("section_keep_positive", False)
generator.show_section()
generator.set_editor_property("auto_rebuild", True)
actors.set_selected_level_actors([main])
editor.set_level_viewport_camera_info(unreal.Vector(3600, 6500, 900), unreal.Rotator(pitch=-8, yaw=-90, roll=0))
assert levels.save_current_level()
report = {"passed": True, "map": MAP, "status": generator.last_build_status, "chunks": [len(segment.chunks) for segment in generator.generated_segments], "largest_chunks": [segment.largest_chunk_triangles for segment in generator.generated_segments]}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "manual_install.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MANUAL_INSTALL " + json.dumps(report))
