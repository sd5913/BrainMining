import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert editor.get_game_world() is None
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
assert generator.hollow_region_prototype
region = generator.hollow_region
original_width = region.region_width
original_point = region.path.get_location_at_spline_point(2, unreal.SplineCoordinateSpace.WORLD)
state = {"started": time.monotonic(), "phase": 0, "revision": generator.build_revision, "after": 0}
report = {"passed": False, "checks": []}
generator.set_editor_property("auto_rebuild", True)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    region.set_editor_property("region_width", original_width)
    region.path.set_location_at_spline_point(2, original_point, unreal.SplineCoordinateSpace.WORLD, True)
    assert generator.rebuild_tunnels(), generator.last_build_status
    report.update({"passed": passed, "reason": reason})
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_region_editor.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_REGION_EDITOR " + json.dumps(report))


def tick(delta):
    try:
        now = time.monotonic()
        if now - state["started"] > 45:
            finish(False, "Editor rebuild/undo timed out")
            return
        if state["phase"] == 0:
            with unreal.ScopedEditorTransaction("Validate Hollow Region Width"):
                region.modify()
                region.set_editor_property("region_width", original_width + 400)
            state["phase"] = 1
        elif state["phase"] in (1, 2, 3, 4) and generator.build_revision > state["revision"] and generator.layout_applied:
            state["revision"] = generator.build_revision
            if state["phase"] == 1:
                assert abs(region.resolved_size.y - original_width - 400) < 1
                report["checks"].append("Property edit automatically rebuilds terrain")
                unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
                state["phase"] = 2
            elif state["phase"] == 2:
                assert abs(region.region_width - original_width) < 1 and abs(region.resolved_size.y - original_width) < 1
                report["checks"].append("Undo restores property and generated geometry")
                unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
                state["phase"] = 3
            elif state["phase"] == 3:
                assert abs(region.resolved_size.y - original_width - 400) < 1
                report["checks"].append("Redo restores edited geometry")
                with unreal.ScopedEditorTransaction("Validate Hollow Spline Point"):
                    region.modify()
                    region.path.modify()
                    region.path.set_location_at_spline_point(2, original_point + unreal.Vector(0, 180, 0), unreal.SplineCoordinateSpace.WORLD, True)
                state["phase"] = 4
            else:
                report["checks"].append("Spline point edit automatically rebuilds terrain")
                finish(True, "Automatic editing, Undo and Redo passed; original prototype restored")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
