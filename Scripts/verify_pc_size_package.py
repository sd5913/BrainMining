import argparse
import json
import subprocess
import time
from pathlib import Path

from packaged_qa_client import Client


def verify(executable, output):
    output = output.resolve()
    profile = output / "profile"
    if output.exists():
        raise ValueError("Use a new verification output directory")
    output.mkdir(parents=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    started = time.monotonic()
    process = subprocess.Popen([
        str(executable.resolve()), "-windowed", "-ResX=1600", "-ResY=900",
        "-RenderOffscreen", "-NoSplash", "-MiningQA", "-UserDir=" + str(profile),
    ], startupinfo=startup)
    client = Client(profile)
    checks = []
    states = {}
    screenshots = []
    report = {"passed": False, "scope": "Automated startup, UI, tools, room traversal fixtures, ending entry and cinematic preview; not a continuous playthrough.", "executable": str(executable.resolve())}

    def command(operation, **fields):
        if process.poll() is not None:
            raise RuntimeError(f"Packaged process exited: {process.returncode}")
        result = client.command(operation, **fields)
        if not result["ok"]:
            raise RuntimeError(f"QA command failed: {operation}: {result}")
        return result

    def wait_state(predicate, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = command("state", timeout=min(60, timeout))
            if predicate(state):
                return state
            time.sleep(0.25)
        raise TimeoutError("Packaged state did not reach the expected checkpoint")

    def check(condition, label):
        if not condition:
            raise AssertionError(label)
        checks.append(label)
        print("PASS " + label, flush=True)

    def capture(name):
        time.sleep(3)
        command("shot", name=name)
        path = client.directory / (name + ".png")
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline and not path.is_file():
            time.sleep(0.2)
        if not path.is_file():
            raise RuntimeError(f"Screenshot was not written: {path}")
        screenshots.append(str(path))

    try:
        state = wait_state(lambda state: state["opening"] and len(state["menu_buttons"]) >= 2)
        report["startup_seconds"] = time.monotonic() - started
        labels = [button["text"] for button in state["menu_buttons"]]
        check("开始游戏" in labels and "退出游戏" in labels, "fresh profile has Chinese Start and Exit labels")
        states["menu"] = state
        capture("01-menu")
        command("start")
        state = wait_state(lambda state: not state["opening"])
        check(state["rendering"] and not state["world_render_disabled"], "main game world renders after Start")
        states["main"] = state
        capture("02-main")
        for tool in (0, 1, 2):
            command("gm_tool", tool=tool)
        state = command("state")
        check(state["bomb"] and state["drill"], "bomb and drill unlock through developer fixture")
        for area, name in ((1, "office"), (3, "echo-cards"), (4, "third-mine"), (5, "museum")):
            command("area_fixture", area=area)
            state = wait_state(lambda state: state["area"] == area)
            states[name] = state
            check(state["rendering"] and not state["world_render_disabled"], name + " area enters and renders")
            if area == 1:
                device = command("actors", filter="PopupOfficeDevice")["actors"][0]
                target = [coordinate + (124 if axis == 2 else 0) for axis, coordinate in enumerate(device["position"])]
                eye = [coordinate - device["forward"][axis] * 215 + (158 if axis == 2 else 0) for axis, coordinate in enumerate(device["position"])]
                command("pose", eye=eye, target=target)
            elif area == 3:
                stage = command("actors", filter="CardRoomStage")["actors"][0]
                target = stage["position"]
                command("pose", eye=[target[0] - 75, target[1], target[2] + 115], target=target)
            capture(f"area-{area}-{name}")
        command("pause")
        state = wait_state(lambda state: state["menu_open"])
        check(state["menu_open"], "pause menu opens in packaged game")
        capture("07-pause")
        command("pause")
        state = wait_state(lambda state: not state["menu_open"])
        # The developer list changes when standalone test maps are removed.
        # Resolve the intended map instead of relying on an obsolete list index.
        ending_map = "/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"
        maps = state["gm_maps"]
        if ending_map not in maps:
            raise AssertionError("Ending map is absent from the developer map list")
        command("gm_map", index=maps.index(ending_map))
        state = wait_state(lambda state: state.get("ending_gate") == 0)
        states["ending"] = state
        state = command("neurons")
        check(state["visible_cell_parts"] > 0 and state["visible_blood_parts"] > 0, "ending entry loads visible cell and blood scenery")
        capture("08-ending-cell")
        command("film_fixture")
        for shot, name in ((1, "street"), (3, "planet"), (4, "cosmos")):
            state = wait_state(lambda state: state.get("ending_shot", -1) >= shot, timeout=240)
            check(state["ending_shot"] == shot, name + " cinematic checkpoint reached")
            states["ending-" + name] = state
            capture("ending-" + name)
        command("quit")
        process.wait(timeout=60)
        check(process.returncode == 0, "packaged game exits cleanly")
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
        report.update(checks=checks, states=states, screenshots=screenshots, elapsed_seconds=time.monotonic() - started)
        (output / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    options = parser.parse_args()
    verify(options.executable, options.output)
