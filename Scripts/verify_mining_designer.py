import copy
import json
import re
import traceback
import uuid
from pathlib import Path

import unreal


library = unreal.MiningDesignerLibrary
sections = unreal.MiningDesignerSection
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None, "Stop PIE first. This script never saves or loads maps."
generators = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MiningTunnelGenerator)]
assert len(generators) == 1, "Select a test level with exactly one generator."
generator = generators[0]
chambers = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MiningHollowChamber)]
rooms = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.MiningLayoutRoom)]
assert chambers and rooms, "Use a hollow-region level with a chamber and rooms."
chamber = chambers[0]
room = rooms[0]
report_path = Path(unreal.Paths.project_saved_dir()) / "Validation" / "mining_designer_presets.json"
report_path.parent.mkdir(parents=True, exist_ok=True)
report = {"passed": False, "checks": [], "scope": "Real editor actors; all test edits restored; no map save/load."}
original_selection = actors.get_selected_level_actors()
original_auto_rebuild = generator.get_editor_property("auto_rebuild")
test_name = "DesignerTest_" + uuid.uuid4().hex
preset_directory = Path(unreal.Paths.project_config_dir()).resolve() / "MiningDesignerPresets"
created_paths = []
originals = []


def check(label, condition):
    assert condition, label
    report["checks"].append(label)


def capture(target, section):
    return json.loads(library.capture_preset(target, section))


def apply(target, section, preset):
    return library.apply_preset(target, section, json.dumps(preset)) is not None


def set_struct(preset, struct_name, field, value):
    result = copy.deepcopy(preset)
    text, count = re.subn(r"\b" + field + r"=[^,)]+", field + "=" + str(value), result["values"][struct_name])
    assert count == 1, field
    result["values"][struct_name] = text
    return result


def fingerprint(target):
    location = target.get_actor_location()
    scale = target.get_actor_scale3d()
    result = {"location": [location.x, location.y, location.z], "scale": [scale.x, scale.y, scale.z]}
    if isinstance(target, unreal.MiningHollowChamber):
        result["region"] = target.region.get_path_name()
        result["manual"] = target.get_editor_property("manual_branch_path")
        path = target.get_editor_property("path")
        result["points"] = []
        for index in range(path.get_number_of_spline_points()):
            point = path.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.LOCAL)
            result["points"].append([point.x, point.y, point.z])
    if isinstance(target, unreal.MiningLayoutRoom):
        result["order"] = target.connection_order
        result["layout"] = str(target.layout_id)
    return result


try:
    for section in [sections.GAMEPLAY, sections.MINING, sections.TERRAIN, sections.VISUAL, sections.ADVANCED]:
        originals.append((generator, section, library.capture_preset(generator, section)))
    for target in [chamber, room]:
        originals.append((target, sections.TERRAIN, library.capture_preset(target, sections.TERRAIN)))
    generator.set_editor_property("auto_rebuild", False)
    gameplay = capture(generator, sections.GAMEPLAY)
    changed = set_struct(gameplay, "FlightSettings", "AscentSpeed", 725)
    revision = generator.build_revision
    check("Apply flight preset to actual actor", apply(generator, sections.GAMEPLAY, changed))
    check("Flight value is 725", abs(generator.flight_settings.ascent_speed - 725) < 0.01)
    check("Gameplay changes do not rebuild terrain", generator.build_revision == revision)
    mining_preset = capture(generator, sections.MINING)
    check("Preset explicitly includes disabled switches", "bOverrideCharacter=" in mining_preset["values"]["DiggingSettings"])
    disabled = set_struct(mining_preset, "DiggingSettings", "bOverrideCharacter", "False")
    disabled = set_struct(disabled, "DiggingSettings", "DownwardOffset", 0)
    check("Apply disabled switch and zero value", apply(generator, sections.MINING, disabled))
    disabled = capture(generator, sections.MINING)
    enabled = set_struct(disabled, "DiggingSettings", "bOverrideCharacter", "True")
    enabled = set_struct(enabled, "DiggingSettings", "DownwardOffset", 50)
    check("Enable mining override", apply(generator, sections.MINING, enabled))
    check("Restore false and zero from captured preset", apply(generator, sections.MINING, disabled) and not generator.digging_settings.override_character and generator.digging_settings.downward_offset == 0)
    check("Restore mining fixture", apply(generator, sections.MINING, mining_preset))
    baseline = capture(generator, sections.GAMEPLAY)
    invalid = set_struct(changed, "PlayerScaleSettings", "Scale", 0.85)
    invalid = set_struct(invalid, "FlightSettings", "AscentSpeed", 99999)
    check("Reject out-of-range nested field", not apply(generator, sections.GAMEPLAY, invalid))
    check("Rejected preset is all-or-nothing", capture(generator, sections.GAMEPLAY) == baseline)
    for label, key, value in [("Wrong version", "version", 2), ("Wrong family", "family", "/Script/BrainMining.MiningLayoutRoom"), ("Wrong section", "section", 1)]:
        invalid = copy.deepcopy(changed)
        invalid[key] = value
        check(label, not apply(generator, sections.GAMEPLAY, invalid))
    for label, text in [("Reject NaN", "nan"), ("Reject Infinity", "inf"), ("Reject malformed numeric", "garbage"), ("Reject below minimum", "0")]:
        invalid = set_struct(changed, "FlightSettings", "AscentSpeed", text)
        check(label, not apply(generator, sections.GAMEPLAY, invalid))
    invalid = copy.deepcopy(changed)
    invalid["values"]["FlightSettings"] += " trailing-data"
    check("Reject trailing property text", not apply(generator, sections.GAMEPLAY, invalid))
    invalid = copy.deepcopy(changed)
    invalid["values"]["UnexpectedField"] = "1"
    check("Reject unknown root field", not apply(generator, sections.GAMEPLAY, invalid))
    invalid = copy.deepcopy(changed)
    invalid["values"]["FlightSettings"] = "(UnexpectedField=1)"
    check("Reject unknown nested field", not apply(generator, sections.GAMEPLAY, invalid))
    invalid["values"]["FlightSettings"] = "(AscentSpeed=700,AscentSpeed=800)"
    check("Reject duplicate nested field", not apply(generator, sections.GAMEPLAY, invalid))
    region_preset = capture(generator.hollow_region, sections.TERRAIN)
    check("Region preset contains editable plug list", "Plugs" in region_preset["values"])
    oversized_plugs = copy.deepcopy(region_preset)
    oversized_plugs["values"]["Plugs"] = "(" + ",".join(["(Position=0.5,Thickness=240,bEnabled=True)"] * 65) + ")"
    check("Reject oversized plug list before applying", not apply(generator.hollow_region, sections.TERRAIN, oversized_plugs))
    check("Rejected plug list preserves all region parameters", capture(generator.hollow_region, sections.TERRAIN) == region_preset)
    check("Reject oversized clipboard", library.apply_preset(generator, sections.GAMEPLAY, " " * 131073) is None)
    advanced = capture(generator, sections.ADVANCED)
    advanced["values"]["SegmentTriangleBudget"] = "30001"
    check("Do not raise 30000 triangle limit", not apply(generator, sections.ADVANCED, advanced))
    advanced["values"]["SegmentTriangleBudget"] = "30000"
    advanced["values"]["CompletedSegmentPolicy"] = "InvalidPolicy"
    check("Reject invalid enum", not apply(generator, sections.ADVANCED, advanced))
    terrain = capture(chamber, sections.TERRAIN)
    check("Chamber preset excludes ownership and manual path mode", "Region" not in terrain["values"] and "bManualBranchPath" not in terrain["values"])
    terrain["values"]["Region"] = "None"
    check("Reject reference injection", not apply(chamber, sections.TERRAIN, terrain))
    room_preset = capture(room, sections.TERRAIN)
    check("Room preset excludes connection order and identity", "ConnectionOrder" not in room_preset["values"] and "LayoutId" not in room_preset["values"])
    before = fingerprint(chamber)
    check("Chamber reset succeeds", library.reset_section(chamber, sections.TERRAIN) is not None)
    check("Reset preserves chamber transform, ownership and spline", fingerprint(chamber) == before)
    before = fingerprint(room)
    check("Room reset succeeds", library.reset_section(room, sections.TERRAIN) is not None)
    check("Reset preserves room transform and order", fingerprint(room) == before)
    check("Save named preset", library.save_preset(generator, sections.GAMEPLAY, test_name, False) is not None)
    preset_path = preset_directory / ("MiningTunnelGenerator-0-" + test_name + ".json")
    created_paths.append(preset_path)
    saved_bytes = preset_path.read_bytes()
    check("List only matching presets", test_name in library.list_presets(generator, sections.GAMEPLAY) and test_name not in library.list_presets(generator, sections.MINING))
    check("Do not silently overwrite", library.save_preset(generator, sections.GAMEPLAY, test_name, False) is None and preset_path.read_bytes() == saved_bytes)
    check("Restore prior values", apply(generator, sections.GAMEPLAY, gameplay))
    check("Load named preset", library.load_preset(generator, sections.GAMEPLAY, test_name) is not None)
    check("Loaded value equals saved value", capture(generator, sections.GAMEPLAY) == baseline)
    for invalid_name in ["../escape", "bad/name", "bad:name", "name ", ".", "a" * 65]:
        check("Reject filename " + repr(invalid_name), library.save_preset(generator, sections.GAMEPLAY, invalid_name, False) is None)
    check("Class defaults cannot be edited", not library.can_edit(unreal.get_default_object(unreal.MiningTunnelGenerator)))
    check("Missing preset returns failure", library.load_preset(generator, sections.GAMEPLAY, test_name + "_missing") is None)
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    restoration_errors = []
    for target, section, original in originals:
        try:
            if library.apply_preset(target, section, original) is None:
                restoration_errors.append(target.get_actor_label() + ": " + str(section))
        except Exception:
            restoration_errors.append(traceback.format_exc())
    generator.set_editor_property("auto_rebuild", original_auto_rebuild)
    actors.set_selected_level_actors(original_selection)
    for preset_path in created_paths:
        if preset_path.parent == preset_directory and test_name in preset_path.name:
            preset_path.unlink(missing_ok=True)
    report["restored"] = not restoration_errors
    report["restoration_errors"] = restoration_errors
    report["passed"] = report["passed"] and report["restored"]
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("MINING_DESIGNER_PRESETS " + json.dumps(report, ensure_ascii=False))
