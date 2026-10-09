import gc
import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "developer_map_runtime.json"
output.parent.mkdir(parents=True, exist_ok=True)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpaces_Demo")
levels.editor_set_viewport_realtime(True)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
spaces = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace)
report = {"passed": False, "checks": [], "samples": [], "scope": "Projection, controls and teleport-only 3D locator fixtures; not a traversal test"}


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def signature(layout):
    result = [(region.name, str(region.kind), region.group_index,
             [(point.x, point.y, point.z) for point in region.get_editor_property("vertices")])
            for region in layout.generated_map_regions]
    gc.collect()
    return result


check(generator.rebuild_tunnels(), generator.last_build_status)
baseline = signature(generator)
check(len(baseline) >= generator.room_count + generator.tunnel_count + generator.space_count, "Snapshot includes rooms, full-face tunnels and enabled spaces")
check(generator.segment_triangle_budget == 30000, "Chunk budget unchanged")
old_auto = generator.auto_rebuild
generator.set_editor_property("auto_rebuild", False)
if spaces:
    target = next(space for space in spaces if space.enabled)
    original_size = target.space_size.copy()
    revision = generator.build_revision
    try:
        target.set_editor_property("space_size", unreal.Vector(1, 1, 1))
        check(not generator.rebuild_tunnels(), "Invalid edit rejected")
        check(signature(generator) == baseline and generator.build_revision == revision, "Failed rebuild retains matching minimap snapshot and terrain revision")
    finally:
        target.set_editor_property("space_size", original_size)
        check(generator.rebuild_tunnels(), "Restore original space dimensions")
generator.set_editor_property("auto_rebuild", old_auto)
check(levels.save_current_level(), "Save geometry snapshot without changing authored layout")
state = {"started": time.monotonic(), "phase": 0, "next": time.monotonic() + 2, "actions": [], "index": 0}


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = passed
    report["reason"] = reason
    report["elapsed"] = time.monotonic() - state["started"]
    if "pawn" in state:
        pawn = state["pawn"]
        pawn.get_component_by_class(unreal.MiningComponent).reset_field()
        pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
        state["layout"].set_actor_tick_enabled(True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("DEVELOPER_MAP_VALIDATION " + json.dumps(report))
    state.clear()
    gc.collect()


def sample(name, expected):
    minimap = state["map"]
    check(expected(minimap), name)
    check(minimap.map_minimum.x <= minimap.player_map_position.x <= minimap.map_maximum.x and
          minimap.map_minimum.y <= minimap.player_map_position.y <= minimap.map_maximum.y, name + ": marker clipped inside map")
    report["samples"].append({"test": name, "location": minimap.location_label,
                              "view": str(minimap.view), "offscreen": minimap.player_offscreen})


def tick(delta):
    try:
        now = time.monotonic()
        if now - state["started"] > 180:
            raise RuntimeError("Timed out waiting for rendered HUD; keep the PIE viewport active")
        if now < state["next"]:
            return
        state["next"] = now + 0.5
        if state["phase"] == 0:
            levels.editor_request_begin_play()
            state["phase"] = 1
        elif state["phase"] == 1:
            game = editor.get_game_world()
            if game is None:
                return
            controller = unreal.GameplayStatics.get_player_controller(game, 0)
            hud = controller.get_hud()
            if hud is None:
                return
            minimap = hud.get_editor_property("developer_map")
            if minimap.region_count == 0:
                return
            pawn = unreal.GameplayStatics.get_player_character(game, 0)
            layout = minimap.layout
            check(not minimap.legacy_snapshot, "PIE uses serialized successful-build snapshot")
            check(minimap.region_count == len(layout.generated_map_regions), "All snapshot regions cached")
            check(signature(layout) == signature(generator), "PIE snapshot matches editor snapshot")
            state.update({"pawn": pawn, "map": minimap, "layout": layout,
                          "triangles": [segment.triangle_count for segment in layout.generated_segments],
                          "revision": layout.build_revision, "default_view": minimap.view})
            pawn.character_movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            layout.set_actor_tick_enabled(False)
            if not minimap.visible:
                minimap.toggle_map()
            state["actions"] = [
                ("Cycle from 3D to X/Z", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.SIDE_XZ),
                ("Cycle to top", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.TOP_XY),
                ("Expand", minimap.toggle_expanded, lambda item: item.expanded),
                ("Follow player", minimap.toggle_follow, lambda item: item.follow_player),
                ("Zoom in", minimap.zoom_in, lambda item: item.zoom > 1),
                ("Zoom out", minimap.zoom_out, lambda item: abs(item.zoom - 1) < 0.001),
                ("Labels off", minimap.toggle_labels, lambda item: not item.labels),
                ("Labels on", minimap.toggle_labels, lambda item: item.labels),
                ("Cycle to Y/Z", minimap.cycle_view, lambda item: item.view == unreal.MiningMapView.SIDE_YZ),
                ("Cycle back to 3D", minimap.cycle_view, lambda item: item.view == state["default_view"]),
            ]
            for region in layout.generated_map_regions:
                if region.kind not in (unreal.MiningMapRegionKind.ROOM, unreal.MiningMapRegionKind.SPACE):
                    continue
                vertices = list(region.get_editor_property("vertices"))
                center = unreal.Vector(sum(point.x for point in vertices) / len(vertices),
                                       sum(point.y for point in vertices) / len(vertices),
                                       sum(point.z for point in vertices) / len(vertices))
                state["actions"].append(("Locate " + region.name,
                                          lambda position=center: pawn.set_actor_location(position, False, True),
                                          lambda item, name=region.name: item.location_label == name))
            state["actions"].extend([
                ("Far outside layout", lambda: pawn.set_actor_location(unreal.Vector(100000, 100000, 100000), False, True), lambda item: item.current_region_index == -1),
                ("Fit clamps offscreen marker", minimap.toggle_follow, lambda item: item.player_offscreen),
                ("Follow recovers offscreen marker", minimap.toggle_follow, lambda item: not item.player_offscreen),
                ("Compact", minimap.toggle_expanded, lambda item: not item.expanded),
                ("Restore fit", minimap.toggle_follow, lambda item: not item.follow_player),
            ])
            state["phase"] = 2
        elif state["phase"] == 2:
            if state["index"] >= len(state["actions"]):
                minimap = state["map"]
                minimap.toggle_map()
                check(not minimap.visible, "Hide overlay")
                minimap.toggle_map()
                check(minimap.visible, "Restore overlay")
                check([segment.triangle_count for segment in state["layout"].generated_segments] == state["triangles"], "Overlay never changes triangle counts")
                check(state["layout"].build_revision == state["revision"], "Overlay never regenerates terrain")
                check(all(segment.max_triangles == 30000 for segment in state["layout"].generated_segments), "Per-chunk budget remains 30000")
                finish(True, "All minimap integration checks passed; PIE left running for visual/key validation")
                return
            name, action, expected = state["actions"][state["index"]]
            action()
            state["phase"] = 3
        elif state["phase"] == 3:
            name, action, expected = state["actions"][state["index"]]
            sample(name, expected)
            state["index"] += 1
            state["phase"] = 2
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
