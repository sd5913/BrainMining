import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
regions = list(generator.hollow_regions)
middle = regions[1].from_room
original_location = middle.get_actor_location()
original_radius = regions[2].passage_radius
report = {"passed": False, "checks": []}
state = {"started": time.monotonic(), "phase": 0, "revision": generator.build_revision}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def ready():
    return generator.layout_applied and generator.build_revision > state["revision"]


def finish(passed):
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = passed
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "hollow_chain_editor.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("HOLLOW_CHAIN_EDITOR " + json.dumps(report))
    if "HollowChainAutoQuit" in unreal.SystemLibrary.get_command_line():
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        assert time.monotonic() - state["started"] < 100, "Editor suite timeout: phase %d: %s" % (state["phase"], generator.last_build_status)
        phase = state["phase"]
        if phase == 0:
            with unreal.ScopedEditorTransaction("Test shared hollow room move"):
                middle.modify()
                middle.set_actor_location(original_location + unreal.Vector(100, 0, -30), False, True)
            state["phase"] = 1
        elif phase == 1 and ready():
            check(abs(regions[0].path.get_location_at_spline_point(4, unreal.SplineCoordinateSpace.WORLD).x - (middle.get_actor_location().x - 600)) < 1, "Moving shared room automatically updates incoming region")
            check(abs(regions[1].path.get_location_at_spline_point(0, unreal.SplineCoordinateSpace.WORLD).x - (middle.get_actor_location().x + 600)) < 1, "Moving shared room automatically updates outgoing region")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 2
        elif phase == 2 and ready():
            check((middle.get_actor_location() - original_location).length() < 0.01, "Undo restores shared room and both connected regions")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
            state["phase"] = 3
        elif phase == 3 and ready():
            check((middle.get_actor_location() - original_location).length() > 90, "Redo restores shared room edit")
            middle.set_actor_location(original_location, False, True)
            state["revision"] = generator.build_revision
            state["phase"] = 4
        elif phase == 4 and ready():
            state["revision"] = generator.build_revision
            state["segments"] = list(generator.generated_segments)
            with unreal.ScopedEditorTransaction("Test invalid later region radius"):
                regions[2].modify()
                regions[2].set_editor_property("passage_radius", 900)
            state["phase"] = 5
        elif phase == 5 and not generator.layout_applied:
            check(generator.build_revision == state["revision"] and list(generator.generated_segments) == state["segments"], "Automatic invalid edit keeps every successful region intact")
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 6
        elif phase == 6 and ready():
            check(regions[2].passage_radius == original_radius, "Undo invalid edit restores valid multi-region layout")
            generator.set_editor_property("auto_rebuild", False)
            extra = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(28800, 0, -2700))
            extra.set_editor_property("connection_order", 4)
            extra.set_editor_property("room_size", unreal.Vector(1200, 1200, 1000))
            state["extra"] = extra
            state["revision"] = generator.build_revision
            check(generator.add_missing_hollow_regions(), "Add-missing creates one new link for added room")
            generator.set_editor_property("auto_rebuild", True)
            state["phase"] = 7
        elif phase == 7 and ready():
            check(len(generator.hollow_regions) == 4 and len(generator.generated_segments) == 4, "Added region has independent generated geometry")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 8
        elif phase == 8 and not generator.layout_applied:
            check(len(generator.hollow_regions) == 3 and len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowRegion)) == 3, "Undo add-missing removes only newly created region and references")
            check(generator.build_revision == state["revision"] and len(generator.generated_segments) == 4, "Missing link after undo retains last successful geometry")
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
            state["phase"] = 9
        elif phase == 9 and ready():
            check(len(generator.hollow_regions) == 4 and len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningHollowRegion)) == 4, "Redo restores new editable region and references")
            added = generator.hollow_regions[3]
            generator.set_editor_property("auto_rebuild", False)
            generator.set_editor_property("hollow_regions", regions)
            actors.destroy_actor(added)
            actors.destroy_actor(state["extra"])
            state["revision"] = generator.build_revision
            generator.set_editor_property("auto_rebuild", True)
            state["phase"] = 10
        elif phase == 10 and ready():
            check(len(generator.generated_segments) == 3 and generator.room_count == 4, "Temporary test actors removed and original chain restored")
            check(generator.segment_triangle_budget == 30000, "Editor operations never raise triangle budget")
            finish(True)
    except Exception:
        report["error"] = traceback.format_exc()
        finish(False)


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("HOLLOW_CHAIN_EDITOR_ARMED")
