import json
import math
import time
import traceback
from pathlib import Path

import unreal


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
pawn = unreal.GameplayStatics.get_player_character(world, 0)
mining = pawn.get_component_by_class(unreal.MiningComponent)
movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
mining.reset_field()
movement.disable_movement()
segment = generator.generated_segments[0]
query = unreal.GeometryScript_MeshQueries
vertices = unreal.GeometryScript_List.convert_vector_list_to_array(query.get_all_vertex_positions(segment.chunks[0].initial_mesh, False)[1])
center = sum(vertices, unreal.Vector()) / len(vertices) + segment.get_actor_location()
report = {"passed": False, "checks": []}
state = {"phase": 0, "started": time.monotonic(), "time": time.monotonic()}


def check(condition, label):
    assert condition, label
    report["checks"].append(label)


def cut_series(count):
    for cut in range(count):
        position = center + unreal.Vector(0, (cut - 6) * 60, math.sin(cut * 0.71) * 28)
        check(segment.mine_sphere(position, 130), "Overlapping runtime cut succeeds: " + segment.last_status)
    check(segment.simplification_pending, "Background simplification scheduled")


def snapshot():
    return [(chunk.triangles, chunk.cuts) for chunk in segment.chunks]


def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    mining.reset_field()
    movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
    report.update({"passed": passed, "reason": reason, "elapsed": time.monotonic() - state["started"]})
    (Path(unreal.Paths.project_saved_dir()) / "Validation" / "chunk_async.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("CHUNK_ASYNC " + json.dumps(report))


def tick(delta):
    try:
        if time.monotonic() - state["started"] > 30:
            finish(False, "Async validation timed out")
            return
        if time.monotonic() - state["time"] < 0.5:
            return
        if state["phase"] == 0:
            cut_series(13)
            state["snapshot"] = snapshot()
        elif state["phase"] == 1:
            if segment.simplification_pending:
                return
            check(segment.discarded_simplifications >= 1, "New cuts invalidate stale simplification")
            check(snapshot() == state["snapshot"], "Stale worker cannot restore removed rock")
            mining.reset_field()
            cut_series(12)
            mining.reset_field()
            state["snapshot"] = snapshot()
        elif state["phase"] == 2:
            if segment.simplification_pending:
                return
            check(snapshot() == state["snapshot"] and segment.successful_cuts == 0, "Reset invalidates pending geometry without resurrection")
            check(segment.discarded_simplifications >= 1, "Old-generation result discarded")
            cut_series(12)
        elif state["phase"] == 3:
            if segment.simplification_pending:
                return
            report["removed"] = segment.simplified_triangles
            report["worker_ms"] = segment.last_simplification_milliseconds
            check(segment.completed_simplifications > 0 and segment.simplified_triangles > 0, "Valid background result reduces geometry")
            check(all(query.get_is_closed_mesh(chunk.mesh.get_dynamic_mesh()) for chunk in segment.chunks), "Async result preserves closed chunks")
            check(segment.successful_cuts == 12, "Simplification does not fabricate digging progress")
            finish(True, "Background simplification applies valid results and discards stale/reset results")
            return
        state["phase"] += 1
        state["time"] = time.monotonic()
    except Exception:
        finish(False, traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("CHUNK_ASYNC_ARMED")
