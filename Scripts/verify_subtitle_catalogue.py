"""Exercise subtitle expiry through the real collection UI in an isolated package."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from packaged_qa_client import Client


def verify(executable, output):
    executable, output = executable.resolve(), output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    profile = output / "profile"
    client = Client(profile)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(executable), "-windowed", "-ResX=1600", "-ResY=900",
        "-RenderOffscreen", "-NoSplash", "-NoSteam", "-MiningQA", "-UserDir=" + str(profile)], startupinfo=startup)
    report = dict(passed=False, executable=str(executable), user_dir=str(profile), checks=[], states={})

    def command(op, **fields):
        result = client.command(op, **fields)
        if not result["ok"]:
            raise RuntimeError(f"QA failed: {op}: {result}")
        return result

    def wait(predicate, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = command("state")
            if predicate(state):
                return state
            time.sleep(.1)
        raise TimeoutError(f"Unexpected game state: {state}")

    def check(condition, label):
        if not condition:
            raise AssertionError(label)
        report["checks"].append(label)
        print("PASS " + label, flush=True)

    try:
        wait(lambda s: s["opening"] and s["menu_buttons"])
        command("start")
        wait(lambda s: not s["opening"])
        command("gm_tool", tool=0)
        command("area_fixture", area=0)
        command("subtitle_fixture", text="Collection subtitle expiry regression", duration=4)
        command("key", key="C", down=True)
        command("key", key="C", down=False)
        state = wait(lambda s: s["catalogue_open"], timeout=10)
        report["states"]["opened"] = state
        check(state["world_paused"], "Collection opens and pauses gameplay")
        check(state["subtitle"] == "Collection subtitle expiry regression", "Active subtitle is initially visible in collection")
        command("shot", name="01-collection-subtitle")
        state = wait(lambda s: not s["subtitle"], timeout=10)
        report["states"]["expired"] = state
        check(state["catalogue_open"] and state["world_paused"], "Subtitle expires naturally while collection stays open and gameplay paused")
        command("shot", name="02-collection-expired")
        command("slate_key", key="Escape")
        state = wait(lambda s: not s["catalogue_open"], timeout=10)
        check(not state["world_paused"] and not state["subtitle"], "Closing collection resumes gameplay without reviving expired text")
        command("subtitle_fixture", text="Playback resumes normally", duration=1)
        state = wait(lambda s: not s["subtitle"], timeout=5)
        check(not state["catalogue_open"], "New dialogue still expires normally after closing collection")
        command("quit")
        process.wait(timeout=60)
        check(process.returncode == 0, "Package exits cleanly")
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
        (output / "results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    verify(args.executable, args.output)
