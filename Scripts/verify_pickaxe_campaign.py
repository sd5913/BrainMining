import json
import time
import traceback
from pathlib import Path

import unreal

standalone = "-game" in unreal.SystemLibrary.get_command_line().split()
if not standalone:
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_default_object(unreal.UnrealEditorSubsystem) if standalone else unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = None if standalone else unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "Pickaxe" / "campaign.json"
output.parent.mkdir(parents=True, exist_ok=True)
report = {"passed": False, "checks": [], "scope": "FullJourney first mine; real player and generated terrain; NullRHI"}
report["standalone"] = standalone
state = {"phase": "start", "started": time.monotonic(), "strike": 0}


def check(condition, label):
    if not condition:
        raise AssertionError(label)
    report["checks"].append(label)


def finish(error=None):
    report.update(passed=error is None, error=error, elapsed=time.monotonic() - state["started"])
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.unregister_slate_post_tick_callback(callback)
    unreal.log("PICKAXE_CAMPAIGN_REPORT " + json.dumps(report))
    if standalone:
        unreal.SystemLibrary.execute_console_command(editor.get_game_world(), "quit")
    else:
        levels.editor_request_end_play()
        unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        if time.monotonic() - state["started"] > 100:
            raise TimeoutError("Campaign validation timed out: " + state["phase"])
        phase = state["phase"]
        if phase == "start":
            world = editor.get_game_world() if standalone else editor.get_editor_world()
            check(world is not None and "Lvl_FullJourney" in world.get_path_name(), "Default FullJourney map loads")
            if not standalone:
                levels.editor_request_begin_play()
            state["phase"] = "setup"
            return
        world = editor.get_game_world()
        if not world:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if phase == "setup":
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property("bReady"):
                return
            hall = halls[0]
            areas = hall.get_editor_property("areas")
            area_index = next(index for index, area in enumerate(areas) if str(area.tag) == "BM_Campaign_Mine01")
            for unused in range(len(areas)):
                if hall.get_editor_property("unlocked_area") >= area_index:
                    break
                check(hall.skip_current_room(), "Campaign progression unlocks the next area")
            check(hall.enter_area(area_index, True), "First campaign mine activates")
            placements = hall.get_editor_property("treasure").get_editor_property("AuthoredPlacements")
            placement = next(item for item in placements if str(item.area_tag) == "BM_Campaign_Mine01" and item.hosts)
            origin = placement.surface.translation
            normal = unreal.MathLibrary.transform_location(placement.surface, unreal.Vector(1, 0, 0)) - origin
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            eye_offset = pawn.get_actor_eyes_view_point()[0] - pawn.get_actor_location()
            pawn.set_actor_location(origin + normal * 220 - eye_offset, False, True)
            controller.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0], origin))
            mining.select_pickaxe()
            mining.interval = 0.65
            mining.radius = 100.0
            mining.downward_offset = 0.0
            state.update(phase="target", mining=mining, next=now + 2.0)
            return
        if now < state["next"]:
            return
        mining = state["mining"]
        if phase == "target":
            rock = mining.target_rock
            check(isinstance(rock, unreal.GeneratedMiningRock), "Real player targets campaign generated terrain")
            state.update(rock=rock, baseline=rock.successful_cuts, phase="hit")
            report["rock"] = rock.get_name()
            phase = "hit"
        rock = state["rock"]
        if phase == "hit":
            check(mining.try_mine(), "Campaign swing %d is accepted" % (state["strike"] + 1))
            state.update(phase="impact", next=now + 0.4)
        elif phase == "impact":
            if rock.get_editor_property("bCutPending"):
                return
            state["strike"] += 1
            strike = state["strike"]
            if strike < 3:
                check(rock.successful_cuts == state["baseline"] + strike, "Campaign strike %d changes real terrain" % strike)
                check(mining.target_pickaxe_hits == strike, "Campaign local progress reaches %d of 3" % strike)
                state.update(phase="hit", next=now + 0.5)
            else:
                check(mining.get_editor_property("bLastPickaxeBreak"), "Third strike submits campaign excavation")
                state.update(phase="drain", next=now)
        elif phase == "drain":
            if rock.get_editor_property("bCutPending"):
                return
            check(rock.successful_cuts == state["baseline"] + 3, "Three strikes complete three progressive campaign cuts")
            report.update(cuts=rock.successful_cuts - state["baseline"], touched_chunks=rock.last_touched_chunks,
                          status=rock.last_status, boolean_ms=rock.last_boolean_milliseconds)
            finish()
    except Exception:
        finish(traceback.format_exc())


callback = unreal.register_slate_post_tick_callback(tick)
unreal.log("PICKAXE_CAMPAIGN_VALIDATION_ARMED")
