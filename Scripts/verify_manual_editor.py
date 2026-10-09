import json
import time
import traceback
from pathlib import Path

import unreal

unreal.MiningDesignerLibrary.open_manual_editor()


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualRegion)[0]
main = region.main_passage
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors.set_selected_level_actors([main])
side = next(shape for shape in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualShape) if shape.kind == unreal.MiningManualKind.SIDE_ROOM)
branch = next(shape for shape in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualShape) if shape.kind == unreal.MiningManualKind.PASSAGE and shape != main)
source = [point.export_text() for point in main.boundary_a]
branch_source = [point.export_text() for point in branch.boundary_a]
floor = side.get_actor_location()
size = side.size
generator.set_editor_property("auto_rebuild", True)
report = {"passed": False, "checks": []}
state = {"started": time.monotonic(), "phase": 0, "revision": generator.build_revision}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def ready():
    return generator.layout_applied and generator.build_revision > state["revision"]


def next_phase():
    state["revision"] = generator.build_revision
    state["phase"] += 1


def finish(passed):
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = passed
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "manual_editor.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MANUAL_EDITOR " + json.dumps(report))
    if "ManualAutoQuit" in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        assert time.monotonic() - state["started"] < 120, "Editor validation timed out at phase %d: %s" % (state["phase"], generator.last_build_status)
        phase = state["phase"]
        if phase == 0:
            if time.monotonic() - state["started"] < 2:
                return
            check(actors.get_selected_level_actors() == [main], "Opening canvas retains the externally selected shape instead of its owner region")
            with unreal.ScopedEditorTransaction("Manual validation curve drag"):
                main.modify()
                points = list(main.boundary_a)
                points[2].set_editor_property("leave", unreal.Vector2D(350, 70))
                main.set_editor_property("boundary_a", points)
            next_phase()
        elif phase == 1 and ready():
            check([point.export_text() for point in main.boundary_a] != source, "Tangent edit rebuilds automatically in rendered editor")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
        elif phase == 2 and ready():
            check([point.export_text() for point in main.boundary_a] == source, "Undo restores authored curve and rebuilds valid terrain")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
        elif phase == 3 and ready():
            check([point.export_text() for point in main.boundary_a] != source, "Redo replays authored edit without stale generated pointers")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
        elif phase == 4 and ready():
            check([point.export_text() for point in main.boundary_a] == source, "Source recovered after second undo")
            with unreal.ScopedEditorTransaction("Manual validation room drag"):
                side.modify()
                side.root_component.modify()
                side.set_actor_location(floor + unreal.Vector(80, 0, 30), False, True)
            next_phase()
        elif phase == 5 and ready():
            check([point.export_text() for point in branch.boundary_a] == branch_source, "Side room move never replaces authored branch middle")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
        elif phase == 6 and ready():
            check((side.get_actor_location() - floor).length() < 0.01, "Room transform undo restores portal")
            with unreal.ScopedEditorTransaction("Manual validation room size"):
                side.modify()
                side.set_editor_property("size", unreal.Vector2D(size.x + 100, size.y + 60))
            next_phase()
        elif phase == 7 and ready():
            check((side.get_actor_location() - floor).length() < 0.01, "Changing height keeps room floor anchored")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
        elif phase == 8 and ready():
            check(side.size == size, "Undo restores room dimensions")
            state["segments"] = list(generator.generated_segments)
            with unreal.ScopedEditorTransaction("Manual validation invalid draft"):
                region.modify()
                region.set_editor_property("rock_size", unreal.Vector(100, 100, 100))
            next_phase()
        elif phase == 9 and not generator.layout_applied:
            check(generator.build_revision == state["revision"] and list(generator.generated_segments) == state["segments"], "Invalid draft preserves every previous segment")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
        elif phase == 10 and ready():
            check(region.rock_size == unreal.Vector(6000, 2400, 3600), "Undo invalid region restores authoring and playable map")
            check(generator.segment_triangle_budget == 30000 and len(generator.generated_segments[0].chunks) <= 128, "Editor operations retain original budgets")
            finish(True)
    except Exception:
        report["error"] = traceback.format_exc()
        finish(False)


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("MANUAL_EDITOR_ARMED")
