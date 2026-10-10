import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
report = {"passed": False, "checks": [], "samples": [], "scope": "Rendered PIE map controls and projection; no authored geometry changes or map saves"}
state = {"started": time.monotonic(), "next": time.monotonic() + 3, "phase": 0, "index": 0}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "developer_map_3d_runtime.json"


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def terrain_signature(layout):
    return [(segment.get_path_name(), segment.triangle_count, segment.max_triangles)
            for segment in layout.generated_segments if segment]


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    if "pawn" in state:
        state["pawn"].set_actor_location(state["position"], False, True)
        state["pawn"].character_movement.set_movement_mode(state["movement_mode"])
        state["layout"].set_actor_tick_enabled(state["layout_tick"])
        minimap = state["map"]
        if not minimap.visible:
            minimap.toggle_map()
        minimap.reset_view()
        if not minimap.expanded:
            minimap.toggle_expanded()
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("DEVELOPER_MAP_3D " + json.dumps(report, ensure_ascii=False))


def tick(delta):
    try:
        now = time.monotonic()
        assert now - state["started"] < 150, "Timed out waiting for a rendered PIE HUD"
        if now < state["next"]:
            return
        state["next"] = now + 0.65
        if state["phase"] == 0:
            levels.editor_request_begin_play()
            state["phase"] = 1
            return
        game = editor.get_game_world()
        if game is None:
            return
        if state["phase"] == 1:
            controller = unreal.GameplayStatics.get_player_controller(game, 0)
            hud = controller.get_hud() if controller else None
            if hud is None:
                return
            minimap = hud.get_component_by_class(unreal.MiningDeveloperMapComponent)
            if minimap is None or minimap.region_count == 0 or minimap.display_triangle_count == 0:
                return
            pawn = unreal.GameplayStatics.get_player_character(game, 0)
            layout = minimap.layout
            state.update({"controller": controller, "camera": controller.get_control_rotation(), "map": minimap,
                          "layout": layout, "pawn": pawn, "position": pawn.get_actor_location(),
                          "movement_mode": pawn.character_movement.movement_mode,
                          "layout_tick": layout.is_actor_tick_enabled(), "terrain": terrain_signature(layout),
                          "revision": layout.build_revision, "default_view": minimap.view,
                          "initial_marker": minimap.player_map_position})
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            layout.set_actor_tick_enabled(False)
            check("ORBIT" in str(minimap.view).upper(), "3D is the default in an actual rendered PIE HUD")
            check(minimap.region_count == len(layout.generated_map_regions), "All generated snapshot regions are available in 3D")
            check(minimap.display_triangle_count > 0, "3D faces are projected and ready to draw")
            check(all(item[2] == 30000 for item in state["terrain"]), "Original 30000-per-chunk terrain budget retained")
            state["actions"] = [
                ("Expand", minimap.toggle_expanded, lambda item: item.expanded),
                ("Orbit yaw and elevation", lambda: minimap.orbit_by(70, -20), lambda item: abs(item.orbit_rotation.yaw - 25) < 0.01 and abs(item.orbit_rotation.pitch + 50) < 0.01),
                ("Pan the map", lambda: minimap.pan_by(unreal.Vector2D(500, -300)), lambda item: abs(item.pan_offset.x - 500) < 0.01),
                ("Zoom", minimap.zoom_in, lambda item: item.zoom > 1),
                ("Reset restores readable 3D fit", minimap.reset_view, lambda item: item.zoom == 1 and item.pan_offset.x == 0 and abs(item.orbit_rotation.yaw + 45) < 0.01),
                ("Follow centers the player", minimap.toggle_follow, lambda item: item.follow_player and not item.player_offscreen),
                ("Follow marker is at graph center", lambda: None, lambda item: abs(item.player_map_position.x - (item.map_minimum.x + item.map_maximum.x) / 2) < 1 and abs(item.player_map_position.y - (item.map_minimum.y + item.map_maximum.y) / 2) < 1),
                ("XZ retained", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.SIDE_XZ),
                ("XY retained", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.TOP_XY),
                ("YZ retained", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.SIDE_YZ),
                ("Return to orbit", minimap.cycle_view, lambda item: item.view == state["default_view"] and item.display_triangle_count > 0),
                ("Fit again", minimap.toggle_follow, lambda item: not item.follow_player),
                ("Move outside mapped layout", lambda: pawn.set_actor_location(unreal.Vector(100000, 100000, 100000), False, True), lambda item: item.current_region_index == -1 and item.player_offscreen),
                ("Follow recovers offscreen player", minimap.toggle_follow, lambda item: not item.player_offscreen),
                ("Restore spawn", lambda: pawn.set_actor_location(state["position"], False, True), lambda item: not item.player_offscreen),
                ("Hide", minimap.toggle_map, lambda item: not item.visible),
                ("Show", minimap.toggle_map, lambda item: item.visible),
                ("Compact", minimap.toggle_expanded, lambda item: not item.expanded),
            ]
            state["phase"] = 2
        elif state["phase"] == 2:
            if state["index"] >= len(state["actions"]):
                check(terrain_signature(state["layout"]) == state["terrain"], "Map interaction does not change terrain geometry or budgets")
                check(state["layout"].build_revision == state["revision"], "No runtime terrain rebuild was triggered")
                camera = state["controller"].get_control_rotation()
                check(abs(camera.yaw - state["camera"].yaw) < 0.01 and abs(camera.pitch - state["camera"].pitch) < 0.01, "Programmatic map orbit never rotates the gameplay camera")
                finish(True, "PIE is left running at the original spawn for visual and mouse verification")
                return
            name, action, expected = state["actions"][state["index"]]
            action()
            state["phase"] = 3
        else:
            name, action, expected = state["actions"][state["index"]]
            minimap = state["map"]
            check(expected(minimap), name)
            report["samples"].append({"action": name, "view": str(minimap.view), "triangles": minimap.display_triangle_count,
                                      "location": minimap.location_label, "offscreen": minimap.player_offscreen})
            state["index"] += 1
            state["phase"] = 2
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
