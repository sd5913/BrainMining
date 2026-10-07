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
chambers = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowChamber))
assert len(chambers) == 2
chamber = next(item for item in chambers if not item.hidden_entrance)
original_location = chamber.get_actor_location()
original_size = unreal.Vector(chamber.chamber_size.x, chamber.chamber_size.y, chamber.chamber_size.z)
state = {"started": time.monotonic(), "phase": 0, "revision": generator.build_revision, "changed": 0}
report = {"passed": False, "checks": []}
generator.set_editor_property("auto_rebuild", True)
levels.editor_set_viewport_realtime(True)


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    chamber.set_actor_location(original_location, False, True)
    chamber.set_editor_property("chamber_size", original_size)
    chamber.set_editor_property("manual_branch_path", False)
    chamber.set_editor_property("enabled", True)
    assert generator.rebuild_tunnels(), generator.last_build_status
    assert levels.save_current_level()
    report.update({"passed": passed, "reason": reason})
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_branches_editor.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_BRANCHES_EDITOR " + json.dumps(report))


def tick(delta):
    try:
        now = time.monotonic()
        if now - state["started"] > 80:
            finish(False, "Auto rebuild / undo timeout in phase " + str(state["phase"]))
            return
        if state["phase"] == 0:
            with unreal.ScopedEditorTransaction("Move Hollow Chamber"):
                chamber.modify()
                chamber.set_actor_location(original_location + unreal.Vector(80, 0, 0), False, True)
            state["phase"] = 1
        elif state["phase"] == 6:
            if generator.layout_applied or now - state["changed"] < 0.7:
                return
            assert generator.build_revision == state["revision"] and generator.generated_fill == state["rock"]
            assert state["map"] == [(item.name, tuple((point.x, point.y, point.z) for point in item.vertices)) for item in generator.generated_map_regions]
            report["checks"].append("Invalid drag automatically rejects; old geometry and map retained")
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 7
        elif generator.build_revision > state["revision"] and generator.layout_applied:
            state["revision"] = generator.build_revision
            if state["phase"] == 1:
                end = chamber.path.get_location_at_spline_point(chamber.path.get_number_of_spline_points() - 1, unreal.SplineCoordinateSpace.WORLD)
                assert abs(end.x - original_location.x - 80) < 0.1
                report["checks"].append("Dragging automatically rebuilds branch and cave")
                unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
                state["phase"] = 2
            elif state["phase"] == 2:
                assert (chamber.get_actor_location() - original_location).length() < 0.1
                report["checks"].append("Undo restores cave location and generated terrain")
                unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
                state["phase"] = 3
            elif state["phase"] == 3:
                assert abs(chamber.get_actor_location().x - original_location.x - 80) < 0.1
                report["checks"].append("Redo restores edited cave")
                chamber.set_editor_property("chamber_size", original_size + unreal.Vector(50, 50, 50))
                state["phase"] = 4
            elif state["phase"] == 4:
                report["checks"].append("Chamber size edit automatically updates geometry")
                chamber.reset_branch_path()
                state["revision"] = generator.build_revision
                with unreal.ScopedEditorTransaction("Edit Hollow Branch Bend"):
                    chamber.modify()
                    chamber.path.modify()
                    point = chamber.path.get_location_at_spline_point(1, unreal.SplineCoordinateSpace.WORLD)
                    chamber.path.set_location_at_spline_point(1, point + unreal.Vector(30, 0, 0), unreal.SplineCoordinateSpace.WORLD, True)
                state["phase"] = 5
            elif state["phase"] == 5:
                report["checks"].append("Manual spline bend automatically rebuilds")
                state["rock"] = generator.generated_fill
                state["map"] = [(item.name, tuple((point.x, point.y, point.z) for point in item.vertices)) for item in generator.generated_map_regions]
                with unreal.ScopedEditorTransaction("Reject Invalid Hollow Cave"):
                    chamber.modify()
                    chamber.set_actor_location(original_location + unreal.Vector(0, 10000, 0), False, True)
                state["changed"] = now
                state["phase"] = 6
            elif state["phase"] == 7:
                report["checks"].append("Undo invalid drag restores valid layout automatically")
                chamber.set_editor_property("enabled", False)
                state["phase"] = 8
            elif state["phase"] == 8:
                assert not any(item.name.startswith(chamber.get_actor_label()) for item in generator.generated_map_regions)
                report["checks"].append("Disable removes geometry and map entry automatically")
                chamber.set_editor_property("enabled", True)
                state["phase"] = 9
            elif state["phase"] == 9:
                report["checks"].append("Reenable restores authored chamber")
                finish(True, "Interactive edit, Undo/Redo, invalid rollback and map updates passed")
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
