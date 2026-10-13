import json
import math
import os
import time
import traceback
from pathlib import Path

import unreal


output = Path(unreal.Paths.project_saved_dir()) / "Performance"
output.mkdir(parents=True, exist_ok=True)
label = os.environ.get("BRAINMINING_PROFILE_LABEL", "manual")
quality = os.environ.get("BRAINMINING_PROFILE_QUALITY", "epic")
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if editor.get_game_world() is not None:
    raise RuntimeError("End the current PIE session before starting a performance capture")
settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
old_throttle = settings.get_editor_property("bThrottleCPUWhenNotForeground")
settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state = dict(start=time.perf_counter(), last=time.perf_counter(), phase="start", frames={}, cuts=[], pending=None, original_commands={})


def summarize(values):
    ordered = sorted(values)
    return dict(count=len(values), mean_ms=sum(values) / len(values) if values else 0,
                p95_ms=ordered[max(0, math.ceil(len(values) * .95) - 1)] if values else 0,
                max_ms=max(values, default=0), over_50ms=sum(value > 50 for value in values))


def finish(error=None):
    unreal.unregister_slate_post_tick_callback(handle)
    settings.set_editor_property("bThrottleCPUWhenNotForeground", old_throttle)
    result = dict(label=label, quality=quality, elapsed_seconds=time.perf_counter() - state["start"],
                  measurement="PIE Slate callback intervals, not standalone GPU frame times",
                  frames=state["frames"], summaries={name: summarize(values) for name, values in state["frames"].items()},
                  cuts=state["cuts"], error=error, metadata=state.get("metadata", {}))
    result["cut_calls"] = summarize([cut["call_ms"] for cut in state["cuts"]])
    (output / (label + ".json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("RUNTIME_PERFORMANCE " + json.dumps({"label": label, "error": error, "summaries": result["summaries"]}))
    for name in ("field", "placement", "pawn", "pending"):
        state.pop(name, None)
    levels.editor_request_end_play()
    for name, value in state["original_commands"].items():
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), f"{name} {value}")
    if os.environ.get("BRAINMINING_PROFILE_EXIT", "0") == "1":
        unreal.SystemLibrary.quit_editor()


def tick(delta_seconds):
    now = time.perf_counter()
    gap = (now - state["last"]) * 1000
    state["last"] = now
    try:
        if now - state["start"] > 240:
            raise RuntimeError("Performance capture timed out in " + state["phase"])
        if state["phase"] == "start":
            if now - state["start"] >= 5:
                levels.editor_request_begin_play()
                state.update(phase="warmup", changed=now)
            return
        world = editor.get_game_world()
        if not world:
            return
        if state["phase"] == "warmup":
            if now - state["changed"] < 25:
                return
            field = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTreasureField)[0]
            placement = field.get_editor_property("authored_placements")[0]
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            normal = unreal.MathLibrary.transform_location(placement.surface, unreal.Vector(1, 0, 0)) - placement.surface.translation
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            pawn.set_actor_location(placement.surface.translation + normal * 200, False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_location(), placement.surface.translation))
            tier = 3 if quality == "epic" else 2
            commands = ["t.MaxFPS 0", "r.VSync 0"]
            commands += [f"sg.{group} {tier}" for group in ("GlobalIlluminationQuality", "ReflectionQuality", "ShadowQuality")]
            if quality == "software":
                commands.append("r.Lumen.HardwareRayTracing 0")
            for command in commands:
                name = command.split()[0]
                state["original_commands"][name] = unreal.SystemLibrary.get_console_variable_float_value(name)
                unreal.SystemLibrary.execute_console_command(world, command)
            state.update(field=field, placement=placement, pawn=pawn, phase="settle", changed=now, normal=normal)
            state["metadata"] = dict(hosts=[host.get_name() for host in placement.hosts],
                                     viewport=list(controller.get_viewport_size()),
                                     budgets=[host.get_editor_property("interactive_cut_budget_milliseconds") for host in placement.hosts],
                                     effective_budget_ms=unreal.SystemLibrary.get_console_variable_float_value("bm.Mining.InteractiveBudgetMs"),
                                     commands=commands)
            unreal.log("RUNTIME_PERFORMANCE_READY " + json.dumps(state["metadata"]))
            return
        if state["phase"] == "settle":
            if now - state["changed"] >= 8:
                state.update(phase="idle", changed=now)
            return
        state["frames"].setdefault(state["phase"], []).append(gap)
        if state["phase"] == "idle":
            if now - state["changed"] >= 10:
                state.update(phase="mining", next_cut=now)
            return
        if state["phase"] == "mining":
            placement = state["placement"]
            if state["pending"] is not None:
                if any(host.get_editor_property("cut_pending") for host in placement.hosts):
                    if now - state["pending"]["issued"] > 8:
                        raise RuntimeError("Accepted cut failed to drain")
                    return
                cut = state["pending"]
                cut["completion_ms"] = (now - cut.pop("issued")) * 1000
                cut["successful_after"] = [host.successful_cuts for host in placement.hosts]
                cut["rocks"] = [dict(total_ms=host.last_total_milliseconds, boolean_ms=host.last_boolean_milliseconds,
                                     collision_ms=host.last_collision_milliseconds,
                                     listener_ms=getattr(host, "last_terrain_notification_milliseconds", 0)) for host in placement.hosts]
                state["cuts"].append(cut)
                state.update(pending=None, next_cut=now + .4)
            if now < state["next_cut"]:
                return
            if len(state["cuts"]) >= 18:
                state.update(phase="after_mining", changed=now)
                return
            before = [host.successful_cuts for host in placement.hosts]
            center = placement.surface.translation - state["normal"] * (len(state["cuts"]) * 35)
            started = time.perf_counter()
            accepted = [host.mine_sphere(center, 100) for host in placement.hosts]
            state["pending"] = dict(issued=started, call_ms=(time.perf_counter() - started) * 1000,
                                    accepted=accepted, successful_before=before)
            return
        if state["phase"] == "after_mining" and now - state["changed"] >= 5:
            finish()
    except Exception:
        finish(traceback.format_exc())


handle = unreal.register_slate_post_tick_callback(tick)
unreal.log("RUNTIME_PERFORMANCE_ARMED " + label)
