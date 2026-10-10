import json
import time
import traceback
from pathlib import Path

import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")
world = editor.get_editor_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)[0]
main = region.main_tunnel
side = next(item for item in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialTunnel) if item.kind == unreal.MiningManualKind.SIDE_ROOM)
original = [knot.export_text() for knot in main.knots]
rotation = side.get_actor_rotation()
generator.set_editor_property("auto_rebuild", True)
actors.set_selected_level_actors([main])
unreal.MiningDesignerLibrary.open_spatial_editor()
report = {"passed": False, "checks": []}
state = {"phase": 0, "revision": generator.build_revision, "started": time.monotonic()}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def next_phase():
    state["revision"] = generator.build_revision
    state["phase"] += 1
    unreal.log("SPATIAL_EDITOR_PHASE " + str(state["phase"]))


def undo():
    unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")


def finish(passed):
    unreal.unregister_slate_post_tick_callback(callback)
    report["passed"] = passed
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "spatial_editor.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SPATIAL_EDITOR_TEST " + json.dumps(report))


def tick(delta):
    try:
        assert time.monotonic() - state["started"] < 150, "Editor test timeout: %s %s" % (state["phase"], generator.last_build_status)
        phase = state["phase"]
        ready = generator.layout_applied and generator.build_revision > state["revision"]
        if phase == 0 and time.monotonic() - state["started"] > 3:
            check(actors.get_selected_level_actors() == [main], "Panel retains viewport-selected tunnel")
            with unreal.ScopedEditorTransaction("Spatial XYZ edit validation"):
                main.modify()
                knots = [knot.copy() for knot in main.knots]
                knots[2].set_editor_property("position", knots[2].position + unreal.Vector(0, 30, 20))
                main.set_editor_property("knots", knots)
            next_phase()
        elif phase == 1 and ready:
            check([knot.export_text() for knot in main.knots] != original, "XYZ edit automatically rebuilds")
            next_phase()
            undo()
        elif phase == 2 and ready:
            check([knot.export_text() for knot in main.knots] == original, "Undo restores source and geometry")
            next_phase()
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION REDO")
        elif phase == 3 and ready:
            check([knot.export_text() for knot in main.knots] != original, "Redo restores XYZ edit")
            next_phase()
            undo()
        elif phase == 4 and ready:
            with unreal.ScopedEditorTransaction("Spatial twist and profile validation"):
                main.modify()
                knots = [knot.copy() for knot in main.knots]
                knots[3].set_editor_property("roll", knots[3].roll + 8)
                profile = [point.copy() for point in knots[3].profile]
                profile[0].set_editor_property("position", profile[0].position + unreal.Vector2D(-20, 0))
                knots[3].set_editor_property("profile", profile)
                main.set_editor_property("knots", knots)
            next_phase()
        elif phase == 5 and ready:
            check(main.knots[3].roll == 98, "Per-section roll and asymmetric profile automatically rebuild")
            next_phase()
            undo()
        elif phase == 6 and ready:
            check([knot.export_text() for knot in main.knots] == original, "One undo restores full section edit")
            with unreal.ScopedEditorTransaction("Spatial rotated room validation"):
                side.modify()
                side.root_component.modify()
                side.set_actor_rotation(unreal.Rotator(yaw=28), False)
            next_phase()
        elif phase == 7 and ready:
            check([knot.export_text() for knot in main.knots] == original, "Rotating branch room rebuilds without touching main authoring")
            next_phase()
            undo()
        elif phase == 8 and ready:
            check(abs(side.get_actor_rotation().yaw - rotation.yaw) < 0.001, "Undo restores branch room transform")
            state["segments"] = list(generator.generated_segments)
            with unreal.ScopedEditorTransaction("Spatial invalid authoring validation"):
                region.modify()
                region.set_editor_property("rock_size", unreal.Vector(100, 100, 100))
            next_phase()
        elif phase == 9 and not generator.layout_applied:
            check(generator.build_revision == state["revision"] and list(generator.generated_segments) == state["segments"], "Invalid draft does not replace generated collision actors")
            next_phase()
            undo()
        elif phase == 10 and ready:
            check(generator.layout_applied and [knot.export_text() for knot in main.knots] == original, "Undo invalid draft recovers playable result")
            temporary = actors.spawn_actor_from_class(unreal.MiningSpatialTunnel, unreal.Vector())
            walls = [unreal.MiningSpatialWall.POSITIVE_X, unreal.MiningSpatialWall.NEGATIVE_X, unreal.MiningSpatialWall.POSITIVE_Y, unreal.MiningSpatialWall.NEGATIVE_Y, unreal.MiningSpatialWall.POSITIVE_Z, unreal.MiningSpatialWall.NEGATIVE_Z]
            expected = [(600, 0, 500), (-600, 0, 500), (0, 600, 500), (0, -600, 500), (0, 0, 1000), (0, 0, 0)]
            end = temporary.end_portal.copy()
            end.set_editor_property("room", region.to_room)
            temporary.set_editor_property("end_portal", end)
            for wall, position in zip(walls, expected):
                portal = temporary.start_portal.copy()
                portal.set_editor_property("room", region.from_room)
                portal.set_editor_property("wall", wall)
                temporary.set_editor_property("start_portal", portal)
                check(temporary.fit_portals(), "Six-face portal initialization: " + str(wall))
                check((temporary.knots[0].position - unreal.Vector(*position)).length() < 0.01, "Portal center uses selected XYZ face")
            actors.destroy_actor(temporary)
            actors.set_selected_level_actors([main])
            finish(True)
    except Exception:
        report["error"] = traceback.format_exc()
        finish(False)


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("SPATIAL_EDITOR_TEST_ARMED")
