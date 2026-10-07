import json
import time
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
space = next(space for space in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutSpace) if space.space_name == "DiggableExpansion")
original_size = space.space_size.copy()
state = {"phase": 0, "started": time.monotonic(), "revision": generator.build_revision}
report = {"passed": False, "checks": []}


def check(condition, message):
    assert condition, message
    report["checks"].append(message)


def ready():
    return generator.build_revision > state["revision"] and generator.layout_applied


def finish(passed):
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = passed
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "space_editor.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("SPACE_EDITOR " + json.dumps(report))


def tick(delta):
    try:
        assert time.monotonic() - state["started"] < 60, "Editor edit/undo validation timed out at phase %s: %s" % (state["phase"], generator.last_build_status)
        if state["phase"] == 0:
            check(generator.auto_rebuild, "Auto rebuild enabled")
            with unreal.ScopedEditorTransaction("Test Space Resize"):
                space.modify()
                space.set_editor_property("space_size", original_size + unreal.Vector(150, 0, 0))
            state["phase"] = 1
        elif state["phase"] == 1 and ready():
            check((space.space_size - original_size).length() > 100, "Size edit automatically rebuilt terrain")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 2
        elif state["phase"] == 2 and ready():
            check((space.space_size - original_size).length() < 0.01, "Undo restored size and regenerated geometry")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
            state["phase"] = 3
        elif state["phase"] == 3 and ready():
            check((space.space_size - original_size).length() > 100, "Redo reapplied size and regenerated geometry")
            state["revision"] = generator.build_revision
            duplicate = actors.duplicate_actor(space, world, unreal.Vector(180, 160, 0))
            state["duplicate"] = duplicate
            state["phase"] = 4
        elif state["phase"] == 4 and ready():
            check(generator.space_count == 4, "Duplicate automatically included without manual registration")
            check(state["duplicate"].from_room == space.from_room, "Duplicate preserves explicit tunnel ownership")
            state["revision"] = generator.build_revision
            actors.destroy_actor(state["duplicate"])
            state["phase"] = 5
        elif state["phase"] == 5 and ready():
            check(generator.space_count == 3, "Deletion automatically rebuilt without stale space geometry")
            state["revision"] = generator.build_revision
            added = generator.add_mining_space()
            check(added is not None and added.from_room is not None, "Add Mining Space creates an assigned marker")
            state["added"] = added
            state["phase"] = 6
        elif state["phase"] == 6 and ready():
            check(generator.space_count == 4, "New space auto-generated")
            state["revision"] = generator.build_revision
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
            state["phase"] = 7
        elif state["phase"] == 7 and ready():
            check(generator.space_count == 3, "Undo Add Mining Space removed marker and regenerated terrain")
            space.set_editor_property("space_size", original_size)
            state["revision"] = generator.build_revision
            state["phase"] = 8
        elif state["phase"] == 8 and ready():
            check(generator.segment_triangle_budget == 30000, "Editor operations retain 30000 budget")
            finish(True)
    except Exception:
        report["error"] = traceback.format_exc()
        finish(False)


callback = unreal.register_slate_post_tick_callback(tick)
