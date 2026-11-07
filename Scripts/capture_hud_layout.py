"""Capture the real MainGame HUD in three languages using isolated PIE fixtures.

Launch UnrealEditor with -RenderOffscreen -NoSteam -MiningQA -UserDir=<fresh path>
and -ExecutePythonScript=<this file>. HUD_LAYOUT_OUTPUT may select a fresh report
directory. This checks layout, not natural progression or real Steam achievements.
"""
import json
import os
import time
import traceback
from pathlib import Path
import unreal


command_line = unreal.SystemLibrary.get_command_line().lower()
assert all(flag in command_line for flag in ("-nosteam", "-miningqa", "-userdir=")), "HUD capture requires NoSteam, MiningQA and an isolated UserDir"
root = Path(unreal.Paths.project_dir()).resolve()
output = Path(os.environ.get("HUD_LAYOUT_OUTPUT", str(root / "Saved/Validation/HUDLayout"))).resolve()
output.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
slate = unreal.SlateLibrary
cultures = ["zh-Hans", "en", "zh-Hant"]
# Progress only forwards so the real HUD's completed-objective fade timers remain valid.
scenarios = [
    dict(name="early", level=1, completed=3, prototypes=1, circle=False, circle_done=False),
    dict(name="four-minerals", level=3, completed=31, prototypes=1, circle=False, circle_done=False),
    dict(name="museum-circle", level=3, completed=31, prototypes=4, circle=True, circle_done=False),
    dict(name="max-level", level=4, completed=31, prototypes=4, circle=True, circle_done=True),
    dict(name="no-objectives", level=3, completed=31, prototypes=4, circle=True, circle_done=True),
]
state = dict(phase="load", started=time.monotonic(), next=0, busy=False, scenario=0, culture=0)
report = dict(passed=False, checks=[], images=[], layouts=[],
              scope="Real MainGame rendering; temporary mineral, upgrade and task fixtures. No player save or real achievement validation.")
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def check(value, label):
    if not value:
        raise AssertionError(label)
    report["checks"].append(label)


def step(phase, delay=.5):
    state.update(phase=phase, next=time.monotonic() + delay)


def write(obj, name, value):
    check(helper.write_preview_state(obj, name, str(value)), "Set isolated fixture " + name)


def subsystem(cls, instance):
    return next(item for item in unreal.ObjectIterator(cls) if item.get_outer() == instance)


def hud_widgets(hud):
    def belongs(widget):
        outer = widget.get_outer()
        while outer:
            if outer == hud:
                return True
            outer = outer.get_outer()
        return False
    return {item.get_name(): item for item in unreal.ObjectIterator(unreal.Widget) if belongs(item)}


def vec(value):
    return [float(value.x), float(value.y)]


def geometry(widget):
    cached = widget.get_paint_space_geometry()
    size = slate.get_local_size(cached)
    first = slate.local_to_absolute(cached, unreal.Vector2D(0, 0))
    last = slate.local_to_absolute(cached, unreal.Vector2D(size.x, 0))
    canvas_slot = isinstance(widget.slot, unreal.CanvasPanelSlot)
    transform = widget.get_editor_property("render_transform")
    return dict(size=vec(size), top_left=vec(first), top_right=vec(last),
                shear=vec(transform.shear), angle=float(transform.angle),
                position=vec(widget.slot.get_position()) if canvas_slot else [0, 0],
                slot_size=vec(widget.slot.get_size()) if canvas_slot else vec(size),
                visible=widget.is_visible(), parent=widget.get_parent().get_name())


def inspect_layout():
    widgets = hud_widgets(state["hud"])
    check("CollectionShortcutBackplate" not in widgets, "Collection shortcut remains unframed text")
    check("LeftHUDRetainer" not in widgets, "Left column has no curved projection")
    required = ["LeftHUDStack", "MineralInventoryPanel", "GuidancePanel", "CollectionShortcutPanel", "PickaxeSummaryPanel"]
    for name in required:
        check(name in widgets, "Layout contains " + name)
    layout = {name: geometry(widgets[name]) for name in required}
    inventory = layout["MineralInventoryPanel"]
    guidance = layout["GuidancePanel"]
    shortcut = layout["CollectionShortcutPanel"]
    panels = [inventory, guidance, shortcut]
    # Offscreen Slate does not populate cached geometry for every canvas.
    # Check the shared transform and inspect the captured pixels for alignment.
    check(layout["LeftHUDStack"]["shear"] == [0., 2.], "Shared column preserves the inventory's original tilt")
    shortcut_text = geometry(widgets["CollectionShortcut"])
    check(shortcut_text["shear"] == [0., 0.] and shortcut_text["angle"] == 0.,
          "Unframed collection text inherits the shared tilt without adding another angle")
    check(len({item["parent"] for item in panels}) == 1, "Left panels share one layout parent")
    for item in panels:
        check(item["shear"] == [0., 0.] and item["angle"] == 0.,
              "Left panels inherit one shared tilt without adding separate angles")
        check(abs(item["position"][0] - inventory["position"][0]) < .1,
              "Left panels share a local edge")
        check(abs(item["slot_size"][0] - inventory["slot_size"][0]) < .1,
              "Left panel widths align")
    check(shortcut["position"][1] > inventory["position"][1] + inventory["slot_size"][1],
          "Collection shortcut stays below the mineral inventory")
    if guidance["visible"]:
        check(guidance["position"][1] > inventory["position"][1] + inventory["slot_size"][1],
              "Objective panel has breathing room below inventory")
        check(shortcut["position"][1] > guidance["position"][1] + guidance["slot_size"][1],
              "Collection shortcut follows the full objective panel height")
    rows = []
    for index in range(6):
        row = widgets.get("GuidanceRow" + str(index))
        if row and row.is_visible() and guidance["visible"]:
            item = geometry(row)
            check(item["shear"] == [0., 0.] and item["angle"] == 0.,
                  "Objective text inherits the shared tilt without adding another angle")
            item["text"] = str(row.get_text())
            check(item["position"][1] >= 0 and item["position"][1] + item["slot_size"][1] <= guidance["slot_size"][1] + .1,
                  "Objective text fits within the dynamic panel height")
            if rows:
                previous = rows[-1]
                check(item["position"][1] >= previous["position"][1] + previous["slot_size"][1],
                      "Wrapped objective rows do not overlap")
            rows.append(item)
    scenario = scenarios[state["scenario"]]
    culture = cultures[state["culture"]]
    summary_width = layout["PickaxeSummaryPanel"]["slot_size"][0]
    if scenario["level"] == 4:
        check(summary_width < 200, "Max-level summary contains only the compact ring")
    elif culture == "en":
        check(summary_width > 400, "English retains room for longer mineral requirements")
    else:
        check(summary_width < 400, "Chinese summary restores its compact width")
    if scenario["name"] == "museum-circle":
        check(len(rows) == 1 and bool(rows[0]["text"].strip()), "Museum objective remains visible")
    if scenario["name"] in ("max-level", "no-objectives"):
        check(not guidance["visible"] and not rows, "Completed objectives collapse without an empty card")
    layout.update(scenario=scenario["name"], culture=culture, rows=rows)
    report["layouts"].append(layout)


def finish(error=None):
    report.update(passed=error is None, error=error, elapsed_seconds=time.monotonic() - state["started"])
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    levels.editor_request_end_play()
    step("cleanup", 2)


def tick(delta):
    if state["busy"] or time.monotonic() < state["next"]:
        return
    state["busy"] = True
    try:
        phase = state["phase"]
        if phase == "cleanup":
            if editor.get_game_world():
                return
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        check(time.monotonic() - state["started"] < 300, "Capture remains within its timeout")
        if phase == "load":
            check(not editor.get_game_world(), "No pre-existing PIE session")
            check(levels.load_level("/Game/MainGame/Maps/Lvl_MainGame"), "MainGame loads")
            helper.start_floating_preview()
            step("ready", 6)
            return
        world = editor.get_game_world()
        if not world:
            return
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if not pc or not pc.get_controlled_pawn():
            return
        state["pc"] = pc
        actors = lambda cls: unreal.GameplayStatics.get_all_actors_of_class(world, cls)
        if phase == "ready":
            openings = actors(unreal.MiningOpening)
            if not openings or not helper.read_preview_object(openings[0], "Menu"):
                return
            state.update(opening=openings[0], room=actors(unreal.MiningStartRoom)[0])
            check(helper.resize_preview_viewport(pc, 1920, 1080), "Preview uses 1920x1080")
            instance = unreal.GameplayStatics.get_game_instance(world)
            state["localization"] = subsystem(unreal.MiningLocalizationSubsystem, instance)
            state["saves"] = subsystem(unreal.MiningSaveSubsystem, instance)
            state["wallet"] = subsystem(unreal.MiningWalletSubsystem, instance)
            state["opening"].start_game()
            step("pickup_view", 5)
        elif phase == "pickup_view":
            if not state["opening"].get_editor_property("bFinished"):
                return
            pickup = next(a for a in actors(unreal.MiningStarterPickup) if a.equipment == unreal.MiningStarterEquipment.PICKAXE)
            pawn = pc.get_controlled_pawn()
            camera = pawn.get_component_by_class(unreal.CameraComponent)
            target = pickup.get_actor_location()
            eye = target + unreal.Vector(-100, 0, 60)
            pawn.set_actor_location(eye - (camera.get_world_location() - pawn.get_actor_location()), False, True)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target))
            step("pickup")
        elif phase == "pickup":
            check(state["room"].interact(pc), "Real starter pickaxe interaction succeeds")
            check(state["room"].get_editor_property("bPickaxeUnlocked"), "HUD has a legitimate starter-pickaxe unlock")
            check(helper.prepare_collection_preview_area(pc, 1, False), "Office fixture provides a consistent gameplay background")
            pawn = pc.get_controlled_pawn()
            office = next(actor for actor in actors(unreal.MiningRoomEscape) if actor.get_editor_property("bAreaManaged"))
            target = office.get_editor_property("computer_entry")
            eye = target + unreal.Vector(-1700, -800, -200)
            camera = pawn.get_component_by_class(unreal.CameraComponent)
            pawn.set_actor_location(eye - (camera.get_world_location() - pawn.get_actor_location()), False, True)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target))
            state["hud"] = helper.read_preview_object(pc, "MiningGUI")
            check(state["hud"] is not None, "Actual player HUD is available")
            wallet = state["wallet"]
            for mineral, amount in [("2", 23), ("4", 18), ("6", 6), ("9", 0)]:
                if amount:
                    wallet.add_mineral(mineral, amount)
            write(wallet, "CollectedMineralTotals", helper.read_preview_state(wallet, "Minerals"))
            step("scenario", 2)
        elif phase == "scenario":
            scenario = scenarios[state["scenario"]]
            write(state["wallet"], "UpgradeLevels", "(" + str(scenario["level"]) + ",0,0,0)")
            for name, value in dict(TutorialActive=True, TutorialSoilPassed=True,
                                    TutorialCompleted=scenario["completed"], PrototypesFound=scenario["prototypes"],
                                    MuseumCircleTaskStarted=scenario["circle"], MuseumCircleTaskCompleted=scenario["circle_done"]).items():
                write(state["saves"], name, value)
            state["culture"] = 0
            # Advancing three prototype milestones plays their completion fades
            # in sequence; wait for all of them before the single-circle case.
            step("language", 5.2 if scenario["name"] == "museum-circle" else 3.2)
        elif phase == "language":
            check(state["localization"].set_language_preference(cultures[state["culture"]]), "Switch active HUD language")
            step("capture", .8)
        elif phase == "capture":
            name = scenarios[state["scenario"]]["name"] + "-" + cultures[state["culture"]] + ".png"
            check(helper.capture_preview(pc, str(output / name)), "Capture " + name)
            report["images"].append(str(output / name))
            # Capture first so the report includes visual evidence for checks.
            inspect_layout()
            state["culture"] += 1
            if state["culture"] < len(cultures):
                step("language", .1)
            else:
                state["scenario"] += 1
                if state["scenario"] < len(scenarios):
                    step("scenario", .1)
                else:
                    finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        state["busy"] = False


handle = unreal.register_slate_post_tick_callback(tick)
