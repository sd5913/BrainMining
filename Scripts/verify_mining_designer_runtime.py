import json
import traceback
from pathlib import Path

import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
assert world is not None, "Start PIE first. This script does not change authored parameters or save maps."
editor_generator = next(actor for actor in unreal.ObjectIterator(unreal.MiningTunnelGenerator) if actor.get_path_name().startswith("/Game/") and ":PersistentLevel." in actor.get_path_name() and "UEDPIE_" not in actor.get_path_name())
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
pawn = unreal.GameplayStatics.get_player_character(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
movement = pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
report = {"passed": False, "checks": [], "scope": "Actual PIE pawn values, one-time scaling and editor API lockout."}
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "mining_designer_runtime.json"


def check(label, condition):
    assert condition, label
    report["checks"].append(label)


def near(actual, expected):
    return abs(actual - expected) < 0.001


try:
    settings = generator.digging_settings
    scale_settings = generator.player_scale_settings
    check("Opt-in mining configuration reaches PIE", settings.override_character)
    scale = scale_settings.scale if scale_settings.enabled else 1.0
    radius_multiplier = scale_settings.mining_radius_multiplier if scale_settings.enabled else 1.0
    reach_multiplier = scale_settings.mining_reach_multiplier if scale_settings.enabled else 1.0
    offset_multiplier = scale_settings.mining_downward_offset_multiplier if scale_settings.enabled else 1.0
    check("Mining radius is scaled exactly once", near(mining.radius, settings.radius * scale * radius_multiplier))
    check("Mining reach is scaled exactly once", near(mining.reach, settings.reach * scale * reach_multiplier))
    check("Downward offset is scaled exactly once", near(mining.downward_offset, settings.downward_offset * scale * offset_multiplier))
    check("Interval is not scaled", near(mining.interval, settings.interval))
    check("Inset fraction is applied", near(mining.inset_fraction, settings.inset_fraction))
    check("Zero disables fall-reset as authored", settings.fall_reset_distance != 0 or mining.fall_reset_distance == 0)
    check("Flight ascent setting reaches pawn", near(movement.flight_settings.ascent_speed, generator.flight_settings.ascent_speed))
    check("Flight size scale remains single application", near(movement.flight_size_scale, scale))
    check("Triangle budget is not increased", generator.segment_triangle_budget <= 30000)
    check("Editor actor locked while playing", not unreal.MiningDesignerLibrary.can_edit(editor_generator))
    check("PIE actor cannot be edited by planner", not unreal.MiningDesignerLibrary.can_edit(generator))
    check("Preset capture cannot leak temporary PIE state", unreal.MiningDesignerLibrary.capture_preset(generator, unreal.MiningDesignerSection.MINING) == "")
    before = editor_generator.digging_settings.radius
    preset = {"version": 1, "family": "/Script/BrainMining.MiningTunnelGenerator", "section": 1, "values": {"DiggingSettings": "(bOverrideCharacter=True,Radius=200)"}}
    check("Preset application is blocked during PIE", unreal.MiningDesignerLibrary.apply_preset(editor_generator, unreal.MiningDesignerSection.MINING, json.dumps(preset)) is None)
    check("Blocked application leaves authoring data intact", near(editor_generator.digging_settings.radius, before))
    report["actual"] = {"radius": mining.radius, "reach": mining.reach, "interval": mining.interval, "offset": mining.downward_offset, "scale": scale, "ascent_speed": movement.flight_settings.ascent_speed}
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
finally:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("MINING_DESIGNER_RUNTIME " + json.dumps(report))
