"""Verify a saved demonstration in a fresh editor process without regeneration."""
import json
from pathlib import Path
import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
regions = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExternalRegion)
assert len(regions) == 1
region = regions[0]
rock, shell = region.generated_rock, region.generated_shell
assert rock and shell
assert rock.get_owner() == region and shell.get_owner() == region
assert isinstance(rock, unreal.DiggableRock) and not isinstance(shell, unreal.DiggableRock)
assert rock.chunked and 0 < len(rock.chunks) <= 128
assert all(c.initial_mesh.get_triangle_count() > 0 for c in rock.chunks)
expected = sum(c.initial_mesh.get_triangle_count() for c in rock.chunks)
assert rock.triangle_count == expected
assert all(c.mesh.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION for c in rock.chunks)

def hit(start, end):
    return unreal.SystemLibrary.line_trace_single(world, unreal.Vector(*start), unreal.Vector(*end),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)

# A through-trace must pass through both ports without hitting a leftover bedrock cap.
assert hit((-3500, 0, 800), (3500, 0, 800)) is None
assert hit((-2000, 0, 800), (-2000, 1200, 800)) is not None
assert hit((0, 2500, 1500), (0, 2100, 1500)) is not None
assert rock.mine_sphere(unreal.Vector(-2000, 410, 800), 160)
assert rock.successful_cuts == 1
region.reset_excavation()
assert rock.triangle_count == expected and rock.successful_cuts == 0
assert hit((-3500, 0, 800), (3500, 0, 800)) is None
report = {"passed": True, "scope": "Fresh editor process reload, no generation",
          "chunks": len(rock.chunks), "triangles": expected,
          "ports_collision_open": True, "bedrock_collision_blocks": True,
          "dig_and_reset": True}
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
(output / "external_mining_reload.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("EXTERNAL_MINING_RELOAD " + json.dumps(report))
