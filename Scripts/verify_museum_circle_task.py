"""Verify the mouse-gallery objective in a packaged game with isolated QA saves.

The starter pickaxe and museum mechanisms are fixture-unlocked; entry, HUD, saving, E interaction,
and ending travel exercise the regular game paths. This is not a full playthrough.
"""
import argparse
import json
import subprocess
import time
from pathlib import Path

from packaged_qa_client import Client


TASK = "找到画上奇怪的圆圈"


def verify(executable, output):
    executable, output = executable.resolve(), output.resolve()
    if not executable.is_file():
        raise FileNotFoundError(executable)
    if output.exists():
        raise ValueError("Use a new verification output directory")
    output.mkdir(parents=True)
    profile = output / "profile"
    client = Client(profile)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    started = time.monotonic()
    process = subprocess.Popen([
        str(executable), "-windowed", "-ResX=1600", "-ResY=900", "-culture=zh-Hans",
        "-RenderOffscreen", "-NoSplash", "-NoSteam", "-MiningQA", "-UserDir=" + str(profile),
    ], startupinfo=startup)
    checks, states, screenshots = [], {}, []
    report = dict(passed=False, executable=str(executable), user_dir=str(profile),
                  scope="Fixture-unlocked starter pickaxe and museum; real gallery entry, objective HUD, isolated save/load, E interaction and ending travel.")

    def command(op, **fields):
        if process.poll() is not None:
            raise RuntimeError(f"Packaged process exited: {process.returncode}")
        result = client.command(op, **fields)
        if not result["ok"]:
            raise RuntimeError(f"QA command failed: {op}: {result}")
        return result

    def wait_state(predicate, timeout=120):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            last = command("state", timeout=min(60, timeout))
            if predicate(last):
                return last
            time.sleep(.1)
        states["timeout"] = last
        raise TimeoutError("Packaged game did not reach expected state")

    def check(condition, label):
        if not condition:
            raise AssertionError(label)
        checks.append(label)
        print("PASS " + label, flush=True)

    def task_row(state):
        return next((row for row in state.get("guide_rows", []) if row["index"] == 5), {})

    def capture(name):
        # No settling sleep: completion must be captured before the 2.25s travel.
        command("shot", name=name)
        path = client.directory / (name + ".png")
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline and not path.is_file():
            time.sleep(.05)
        if not path.is_file():
            raise RuntimeError(f"Screenshot was not written: {path}")
        screenshots.append(str(path))

    try:
        wait_state(lambda state: state["opening"] and state["menu_buttons"])
        command("start")
        wait_state(lambda state: not state["opening"])
        # A normal museum player already owns the starter pickaxe. The guide
        # row lives under its HUD panel, so entering via a fixture without it
        # can leave the parent hidden even though the row itself is visible.
        command("gm_tool", tool=0)
        command("area_fixture", area=5)
        state = wait_state(lambda state: state["area"] == 5)
        states["museum-before-gallery"] = state
        check(not state["museum_circle_task_started"] and not state["museum_circle_task_completed"],
              "Entering the main museum does not start or complete the gallery objective")
        portal = next(actor for actor in command("actors", filter="MouseMuseumPortal")["actors"]
                      if actor["class"] == "MouseMuseumPortal")
        origin, cell = portal["position"], portal["cell"]
        command("mouse_gallery_fixture")
        command("pose", eye=[origin[0] + 2100, origin[1] - 350, origin[2] + 60], target=cell)
        state = command("state")
        states["outside-unlocked-gallery"] = state
        check(not state["museum_circle_task_started"], "Unlocking the gallery alone does not start its objective")

        eye = [origin[0] + 2300, origin[1] - 350, origin[2] + 60]
        command("pose", eye=eye, target=[eye[0] - 100, eye[1], eye[2]])
        state = wait_state(lambda state: state["museum_circle_task_started"] and task_row(state).get("visible"))
        states["entered-gallery"] = state
        check(TASK in task_row(state)["text"], "Entering the mouse room displays the requested Chinese objective")
        check(not state["museum_circle_task_completed"], "Entering without interacting does not complete the objective")
        time.sleep(.3)
        capture("01-gallery-objective")

        # An actual save/load round trip uses only this run's fresh -UserDir.
        command("save")
        command("return_menu")
        wait_state(lambda state: state["opening"])
        command("continue")
        state = wait_state(lambda state: not state["opening"] and state["area"] == 5 and state["museum_circle_task_started"])
        states["resumed-gallery-task"] = state
        check(not state["museum_circle_task_completed"], "The active objective survives an isolated save and continue")
        command("pose", eye=eye, target=cell)
        deadline = time.monotonic() + 15
        while True:
            # The Small form changes the live camera offset. Pose places the
            # pawn using a fixed eye height; aim from the actual player view,
            # as a normal mouse movement would, after camera updates settle.
            command("aim", target=cell)
            portal = next(actor for actor in command("actors", filter="MouseMuseumPortal")["actors"]
                          if actor["class"] == "MouseMuseumPortal")
            if portal["focused"]:
                break
            if time.monotonic() >= deadline:
                raise TimeoutError("The painting circle never received actual focus")
            time.sleep(.1)
        state = command("state")
        states["focused-circle"] = state
        check(not state["museum_circle_task_completed"], "Aiming at the real painted circle does not complete the objective")
        check(task_row(state).get("visible", False), "The objective remains visible after loading and focusing")
        capture("02-focused-circle")

        command("key", key="E", down=True)
        command("key", key="E", down=False)
        state = wait_state(lambda state: state["museum_circle_task_completed"] and state.get("task_toast_visible")
                           and TASK in state.get("task_toast", ""), timeout=5)
        states["completed-circle"] = state
        check(state["museum_circle_task_completed"], "Actual E interaction completes the circle objective")
        check(state["task_toast_visible"] and TASK in state["task_toast"], "The standard completion toast displays the circle objective")
        capture("03-circle-complete")
        state = wait_state(lambda state: state.get("map", "").endswith("Lvl_EndingEmergence_Demo")
                           and state.get("ending_gate") == 0)
        states["ending"] = state
        check(state["museum_circle_task_completed"], "Normal painting travel reaches the ending with the objective completed")
        capture("04-ending-arrival")

        command("quit")
        process.wait(timeout=60)
        check(process.returncode == 0, "Packaged game exits cleanly")
        report["passed"] = True
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        if process.poll() is None:
            try:
                client.command("quit", timeout=10)
                process.wait(timeout=20)
            except (TimeoutError, subprocess.TimeoutExpired):
                process.terminate()
                process.wait(timeout=10)
        report.update(checks=checks, states=states, screenshots=screenshots,
                      elapsed_seconds=time.monotonic() - started)
        (output / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    verify(args.executable, args.output)
