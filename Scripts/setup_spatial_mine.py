import json
from pathlib import Path

import unreal


MAP = "/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo"
BASE = "/Game/MiningPrototype/Maps/Lvl_MiningHollow_Demo"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    assert "-SpatialSetupRetry" in unreal.SystemLibrary.get_command_line(), "Do not overwrite authored spatial maps"
    assert levels.load_level(MAP)
    assert not unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), unreal.MiningSpatialRegion), "Existing spatial authoring is protected"
else:
    assert levels.new_level_from_template(MAP, BASE)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("auto_rebuild", False)
generator.set_editor_property("hollow_region", None)
generator.set_editor_property("hollow_regions", [])
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowRegion):
    actors.destroy_actor(actor)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
assert len(rooms) == 2
for room in rooms:
    unreal.log("SPATIAL_ROOM " + room.get_actor_label() + " " + str(room.get_actor_location()) + " " + str(room.room_size) + " " + str(room.floor_anchored))
region = actors.spawn_actor_from_class(unreal.MiningSpatialRegion, unreal.Vector(3600, 0, -100))
region.set_actor_label("三维矿区_范围与主线入口")
region.set_folder_path("SpatialMine/Authoring")
region.set_editor_property("from_room", rooms[0])
region.set_editor_property("to_room", rooms[1])
region.set_editor_property("rock_size", unreal.Vector(6000, 4000, 3600))
region.set_editor_property("curve_tolerance", 8)


def outline(width=500, height=500):
    result = []
    for horizontal, vertical in [(-width / 2, -height / 2), (width / 2, -height / 2), (width / 2, height / 2), (-width / 2, height / 2)]:
        point = unreal.MiningPenPoint()
        point.set_editor_property("position", unreal.Vector2D(horizontal, vertical))
        result.append(point)
    return result


def knots(positions, rolls=None, width=500, height=500):
    result = []
    for index, position in enumerate(positions):
        knot = unreal.MiningSpatialKnot()
        knot.set_editor_property("position", unreal.Vector(*position))
        before = unreal.Vector(*positions[max(0, index - 1)])
        after = unreal.Vector(*positions[min(len(positions) - 1, index + 1)])
        tangent = (after - before) * (0.3 if index in (0, len(positions) - 1) else 0.18)
        knot.set_editor_property("arrive", unreal.Vector(-tangent.x, -tangent.y, -tangent.z))
        knot.set_editor_property("leave", tangent)
        knot.set_editor_property("roll", rolls[index] if rolls else 0)
        knot.set_editor_property("profile", outline(width, height))
        result.append(knot)
    return result


def shape(label, kind, points=None, rolls=None, width=500, height=500):
    actor = actors.spawn_actor_from_class(unreal.MiningSpatialTunnel, unreal.Vector())
    actor.set_actor_label(label)
    actor.set_folder_path("SpatialMine/Authoring")
    actor.set_editor_property("region", region)
    actor.set_editor_property("kind", kind)
    if points:
        actor.set_editor_property("knots", knots(points, rolls, width, height))
    return actor


main = shape("三维主通道_点节点可XYZ拉弯与扭转", unreal.MiningManualKind.PASSAGE,
             [(600, 0, 500), (1600, 300, 500), (2800, 850, 250), (4000, -550, 0), (5300, -850, -200), (6600, 0, -300)],
             [0, 15, 45, 90, 160, 180])
start = main.start_portal
start.set_editor_property("room", rooms[0])
main.set_editor_property("start_portal", start)
end = main.end_portal
end.set_editor_property("room", rooms[1])
main.set_editor_property("end_portal", end)
region.set_editor_property("main_tunnel", main)
side = shape("三维支线房间_可旋转移动改尺寸", unreal.MiningManualKind.SIDE_ROOM)
side.set_actor_location(unreal.Vector(4400, 1350, -500), False, True)
side.set_actor_rotation(unreal.Rotator(yaw=25), False)
side.set_editor_property("room_size", unreal.Vector(900, 800, 800))
branch = shape("三维支线_终点随旋转房间", unreal.MiningManualKind.PASSAGE,
               [(2800, 850, 250), (3992, 1160, -500)], width=340, height=340)
branch_knots = branch.knots
branch_knots[0].set_editor_property("leave", unreal.Vector(500, 100, -300))
branch_knots[1].set_editor_property("arrive", unreal.Vector(-550, -250, 0))
branch.set_editor_property("knots", branch_knots)
end = branch.end_portal
end.set_editor_property("room", side)
end.set_editor_property("wall", unreal.MiningSpatialWall.NEGATIVE_X)
branch.set_editor_property("end_portal", end)
cavity = shape("三维隐藏空洞_独立可编辑", unreal.MiningManualKind.CAVITY,
               [(1700, -1100, -750), (2250, -950, -600)], [0, 35], 700, 600)
cavity.set_editor_property("hidden_cavity", True)
block = shape("三维可挖堵块_普通岩层", unreal.MiningManualKind.ROCK_BLOCK,
              [(3920, -550, 0), (4080, -550, 0)], width=1200, height=1000)
generator.set_actor_label("三维隧道生成器_保持30000预算")
generator.set_editor_property("hollow_regions", [region])
generator.set_editor_property("chunk_length", 1200)
generator.set_editor_property("completed_segment_policy", unreal.CompletedMiningSegments.SEAL_AND_RECYCLE)
assert generator.segment_triangle_budget == 30000
assert generator.rebuild_tunnels(), generator.last_build_status
generator.show_layout()
generator.set_editor_property("auto_rebuild", True)
actors.set_selected_level_actors([main])
editor.set_level_viewport_camera_info(unreal.Vector(1000, -5600, 4300), unreal.Rotator(pitch=-31, yaw=66, roll=0))
assert levels.save_current_level()
report = {"passed": True, "map": MAP, "status": generator.last_build_status,
          "chunks": [len(segment.chunks) for segment in generator.generated_segments],
          "largest_chunks": [segment.largest_chunk_triangles for segment in generator.generated_segments]}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "spatial_install.json"
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("SPATIAL_INSTALL " + json.dumps(report))
