import json
import re
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None, "Stop PIE before the editor scale check"
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
original_settings = generator.get_editor_property("player_scale_settings").copy()
state = {"phase": 0, "started": time.monotonic(), "changed": time.monotonic()}
report = {"passed": False, "phases": []}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "player_scale_editor.json"


def stable(value):
    return re.sub(r" \(0x[0-9A-Fa-f]+\)", "", str(value))


def snapshot():
    rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda room: room.connection_order)
    return {
        "rooms": [{"name": room.get_actor_label(), "transform": stable(room.get_actor_transform()), "size": stable(room.room_size)} for room in rooms],
        "materials": [stable(generator.bedrock_material), stable(generator.tunnel_material)],
        "lighting": [generator.improve_mining_visibility, generator.headlamp_intensity, generator.fill_light_intensity],
        "preview": [stable(generator.preview_mode), generator.section_axis, generator.section_position, generator.section_keep_positive],
        "routes": [[stable(route.start_wall), stable(route.end_wall)] for route in generator.routes],
        "segments": [[segment.get_path_name(), segment.triangle_count] for segment in generator.generated_segments],
        "owned_rocks": sorted(segment.get_path_name() for segment in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GeneratedMiningRock) if segment.get_owner() == generator),
        "budget": generator.segment_triangle_budget,
        "policy": stable(generator.completed_segment_policy),
        "start": stable(generator.generated_player_start.get_actor_transform()),
    }


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    generator.set_editor_property("player_scale_settings", original_settings)
    report.update({"passed": passed, "reason": reason, "after": snapshot()})
    if passed:
        assert unreal.EditorAssetLibrary.save_loaded_asset(world, False)
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([generator])
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("PLAYER_SCALE_EDITOR " + json.dumps(report))


def tick(delta):
    try:
        now = time.monotonic()
        if now - state["started"] > 90:
            finish(False, "Editor scale check timed out")
            return
        if now - state["changed"] < 1 or not generator.layout_applied:
            return
        if state["phase"] == 0:
            report["before"] = snapshot()
            assert len(report["before"]["owned_rocks"]) == len(generator.generated_segments), "Untracked generated rocks remain"
            state["revision"] = generator.build_revision
        else:
            assert generator.build_revision == state["revision"], "Changing player scale regenerated terrain"
            assert snapshot() == report["before"], "Player settings changed room layout or geometry"
            report["phases"].append({"scale": generator.player_scale_settings.scale, "speed_multiplier": generator.player_scale_settings.movement_speed_multiplier, "geometry_unchanged": True})
        variants = [(0.5, 1), (1, 1), (0.7, 1.25), (0.7, 1)]
        if state["phase"] == len(variants):
            finish(True, "Editor settings saved; resizing and tuning do not rebuild or alter the user's terrain")
            return
        scale, speed = variants[state["phase"]]
        settings = unreal.MiningPlayerScaleSettings()
        settings.scale = scale
        settings.movement_speed_multiplier = speed
        generator.set_editor_property("player_scale_settings", settings)
        state["phase"] += 1
        state["changed"] = now
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("PLAYER_SCALE_EDITOR_ARMED")
