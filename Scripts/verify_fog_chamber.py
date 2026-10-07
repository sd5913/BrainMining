import json
from pathlib import Path
import unreal


actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rock = next(actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == "DiggableRock_Main")
chamber = rock.get_component_by_class(unreal.FogChamberComponent)
report = {}


def blocked(start, finish):
    return unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*finish), unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True) is not None


def cut(horizontal, radius, height=170):
    accepted = rock.mine_sphere(unreal.Vector(horizontal, 0, height), radius)
    assert accepted, rock.last_status
    return chamber.get_state_label()


try:
    rock.reset_rock()
    assert chamber.room_ready
    report["sealed_triangles"] = rock.triangle_count
    assert rock.triangle_count == 24
    report["room_is_hollow"] = not blocked((490, 0, 210), (800, 0, 210))
    report["sealed_entrance"] = blocked((0, 0, 210), (650, 0, 210))
    assert report["room_is_hollow"] and report["sealed_entrance"]

    cut(150, 50)
    report["distant_cut_stays_sealed"] = chamber.state == unreal.FogChamberState.SEALED
    assert report["distant_cut_stays_sealed"]
    rock.reset_rock()
    report["near_cut"] = cut(198, 160)
    report["remaining_wall_cm"] = chamber.remaining_wall
    assert chamber.state == unreal.FogChamberState.SEEPING and chamber.breach_count == 0
    assert blocked((198, 0, 170), (650, 0, 170))
    previous_state = chamber.state
    assert not rock.mine_sphere(unreal.Vector(400, 0, 170), -1)
    assert chamber.state == previous_state and chamber.breach_count == 0
    report["rejected_cut_preserves_chamber"] = True
    report["breach_cut"] = cut(360, 160)
    assert chamber.breach_count == 1 and chamber.state == unreal.FogChamberState.VENTING
    assert not blocked((198, 0, 170), (650, 0, 170))
    report["outlet"] = str(chamber.outlet_location)
    report["outlet_direction"] = str(chamber.outlet_direction)
    assert chamber.outlet_direction.x < -0.5
    cut(430, 160)
    assert chamber.breach_count == 1
    report["single_burst_only"] = True

    rock.reset_rock()
    report["internal_pocket"] = cut(390, 115)
    assert chamber.breach_count == 0 and blocked((0, 0, 170), (650, 0, 170))
    report["connecting_internal_pocket"] = cut(190, 150)
    assert chamber.breach_count == 1
    assert not blocked((0, 0, 170), (650, 0, 170))
    report["enclosed_cut_does_not_release_gas"] = True

    rock.reset_rock()
    cut(198, 160)
    report["almost_touching_cut"] = cut(300, 151)
    if blocked((300, 0, 170), (650, 0, 170)):
        assert chamber.breach_count == 0
        report["analytic_sphere_overlap_not_enough"] = True
    else:
        report["analytic_sphere_overlap_not_enough"] = "Actual cutter opened the wall on this mesh"

    rock.reset_rock()
    assert chamber.breach_count == 0 and chamber.state == unreal.FogChamberState.SEALED
    assert rock.triangle_count == 24 and blocked((0, 0, 170), (650, 0, 170))
    report["reset_restores_sealed_room"] = True
    report["passed"] = True
finally:
    rock.reset_rock()
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "fog_chamber_geometry.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("FOG_CHAMBER_GEOMETRY " + json.dumps(report))
