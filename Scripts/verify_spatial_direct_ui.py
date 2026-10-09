import json
from pathlib import Path

import unreal


assert "-SpatialDirectUIValidation" in unreal.SystemLibrary.get_command_line(), "Use only in the dedicated scratch validation session"
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)


def spawn(actor_type, label, position):
    actor = actors.spawn_actor_from_class(actor_type, unreal.Vector(*position))
    actor.set_actor_label(label)
    return actor


def rectangle(width=500, height=500):
    points = []
    for horizontal, vertical in [(-width / 2, -height / 2), (width / 2, -height / 2), (width / 2, height / 2), (-width / 2, height / 2)]:
        point = unreal.MiningPenPoint()
        point.position = unreal.Vector2D(horizontal, vertical)
        points.append(point)
    return points


def knot(position, tangent, width=500, height=500):
    value = unreal.MiningSpatialKnot()
    value.position = unreal.Vector(*position)
    value.leave = unreal.Vector(*tangent)
    value.arrive = value.leave * -1
    value.profile = rectangle(width, height)
    return value


direct_start = spawn(unreal.MiningLayoutRoom, "QA_Start", (0, 0, 500))
direct_finish = spawn(unreal.MiningLayoutRoom, "QA_Finish", (4200, 0, 400))
direct_start.room_size = direct_finish.room_size = unreal.Vector(1200, 1200, 1000)
direct_finish.connection_order = 1
direct_start.set_as_start_room()
direct_region = spawn(unreal.MiningSpatialRegion, "QA_Region", (2100, 0, 500))
direct_region.rock_size = unreal.Vector(3200, 3400, 2400)
direct_region.from_room = direct_start
direct_region.to_room = direct_finish
direct_main = spawn(unreal.MiningSpatialTunnel, "QA_Main", (0, 0, 0))
direct_main.region = direct_region
direct_main.knots = [knot((600, 0, 500), (450, 0, 0)), knot((2100, 0, 450), (500, 0, -25)), knot((3600, 0, 400), (450, 0, 0))]
start_portal = direct_main.start_portal
start_portal.room = direct_start
direct_main.start_portal = start_portal
end_portal = direct_main.end_portal
end_portal.room = direct_finish
direct_main.end_portal = end_portal
direct_region.main_tunnel = direct_main
direct_cavity = spawn(unreal.MiningSpatialTunnel, "QA_Cavity", (2100, 500, 450))
direct_cavity.region = direct_region
direct_cavity.kind = unreal.MiningManualKind.CAVITY
direct_cavity.knots = [knot((-200, 0, 0), (130, 0, 0), 800, 700), knot((200, 0, 0), (130, 0, 0), 800, 700)]
direct_generator = spawn(unreal.MiningTunnelGenerator, "QA_Generator", (0, 0, 0))
direct_generator.auto_rebuild = False
direct_generator.hollow_region_prototype = True
direct_generator.direct_room_faces = True
direct_generator.automatic_chunks = True
direct_generator.hollow_region = direct_region
assert direct_generator.rebuild_tunnels(), direct_generator.last_build_status
actors.set_selected_level_actors([direct_main])
unreal.MiningDesignerLibrary.open_spatial_editor()


def direct_snapshot(name):
    result = {
        "name": name,
        "main_knots": [value.export_text() for value in direct_main.knots],
        "finish_size": direct_finish.room_size.export_text(),
        "finish_location": direct_finish.get_actor_location().export_text(),
        "applied": direct_generator.layout_applied,
        "revision": direct_generator.build_revision,
        "budget": direct_generator.segment_triangle_budget,
        "status": unreal.MiningDesignerLibrary.spatial_editing_status(),
    }
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / ("spatial_direct_ui_" + name + ".json")
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_DIRECT_UI " + json.dumps(result, ensure_ascii=False))


direct_snapshot("initial")
unreal.log("SPATIAL_DIRECT_UI_READY")
