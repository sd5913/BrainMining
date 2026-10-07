import json
import math
import traceback
from pathlib import Path

import unreal


saved = Path(unreal.Paths.project_saved_dir()) / "Validation"
layout = json.loads((saved / "editable_descent_layout.json").read_text(encoding="utf-8"))
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if layout["map"] not in unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name():
    assert levels.load_level(layout["map"])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
all_actors = actor_subsystem.get_all_level_actors()
by_label = {actor.get_actor_label(): actor for actor in all_actors}
rocks = [by_label[entry["name"]] for entry in layout["rocks"]]
report = {"map": layout["map"], "segments": [], "passed": False}


def point(entry, distance, sideways=0, height=140):
    pitch = -math.radians(entry["slope"])
    origin = entry["entrance"]
    return unreal.Vector(origin[0] + math.cos(pitch) * distance - math.sin(pitch) * height, origin[1] + sideways, origin[2] + math.sin(pitch) * distance + math.cos(pitch) * height)


def blocked(start, end, ignored=(), complex_trace=True):
    return unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.ECC_VISIBILITY, complex_trace, list(ignored), unreal.DrawDebugTrace.NONE, True) is not None


try:
    report["section_roots"] = sum(actor.actor_has_tag("SectionRoot") for actor in all_actors)
    report["mineable_rocks"] = len(rocks)
    report["tagged_actors"] = sum(actor.actor_has_tag("EditableDescent") for actor in all_actors)
    assert report["section_roots"] == 7 and len(rocks) == 3
    report["all_parts_attached"] = all(actor.get_attach_parent_actor() is not None for actor in all_actors if actor.actor_has_tag("EditableDescent") and not actor.actor_has_tag("SectionRoot") and str(actor.get_folder_path()) != "Descent/90_Lighting")
    assert report["all_parts_attached"]
    report["runtime_walls_visible"] = all(not actor.get_editor_property("hidden") for actor in all_actors if actor.actor_has_tag("Bedrock"))
    assert report["runtime_walls_visible"]
    for entry, rock in zip(layout["rocks"], rocks):
        root = rock.get_attach_parent_actor()
        origin = root.get_actor_location()
        original_rock = rock.get_actor_location()
        rock.reset_rock()
        detail = {"name": entry["name"]}
        report["segments"].append(detail)
        start, end = point(entry, 20), point(entry, entry["length"] - 20)
        detail["sealed_before_cut"] = blocked(start, end)
        assert detail["sealed_before_cut"]
        midpoint = entry["length"] / 2
        detail["side_hits_complex"] = [blocked(point(entry, midpoint, 0, 220), point(entry, midpoint, sideways, height), [rock]) for sideways, height in [(-420, 220), (420, 220), (0, -150), (0, 620)]]
        detail["side_hits_simple"] = [blocked(point(entry, midpoint, 0, 220), point(entry, midpoint, sideways, height), [rock], False) for sideways, height in [(-420, 220), (420, 220), (0, -150), (0, 620)]]
        detail["walls"] = [{"label": actor.get_actor_label(), "location": str(actor.get_actor_location()), "scale": str(actor.get_actor_scale3d()), "rotation": str(actor.get_actor_rotation()), "collision": str(actor.static_mesh_component.get_collision_enabled())} for actor in root.get_attached_actors() if actor.actor_has_tag("Bedrock")]
        detail["four_fixed_sides"] = all(detail["side_hits_simple"])
        assert detail["four_fixed_sides"]
        detail["cuts"] = []
        for distance in range(60, int(entry["length"]), 105):
            accepted = rock.mine_sphere(point(entry, distance), 160)
            detail["cuts"].append(bool(accepted))
            assert accepted, rock.last_status
        detail["clear_after_cut"] = not blocked(start, end)
        detail["triangles"] = rock.triangle_count
        assert detail["clear_after_cut"]
        rock.reset_rock()
        detail["reset_restores_solid"] = blocked(start, end) and rock.triangle_count == 12
        assert detail["reset_restores_solid"]
        root.set_actor_location(origin + unreal.Vector(0, 125, 0), False, True)
        assert (rock.get_actor_location() - original_rock - unreal.Vector(0, 125, 0)).length() < 0.1
        root.set_actor_location(origin, False, True)
        detail["parent_translation_passed"] = True
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
    raise
finally:
    for rock in rocks:
        rock.reset_rock()
    (saved / "editable_descent_geometry.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("EDITABLE_DESCENT_GEOMETRY " + json.dumps(report))
