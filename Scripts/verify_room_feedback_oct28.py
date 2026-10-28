"""Isolated runtime checkpoints for card tools, stage seams, duck and portal music.

Uses the opt-in QA transport and a new profile; this is not a full playthrough.
Screenshots require visual review in addition to the state assertions.
"""
import argparse
import json
import subprocess
import time
import traceback
from pathlib import Path
from packaged_qa_client import Client

parser = argparse.ArgumentParser()
parser.add_argument("output")
parser.add_argument("--exe")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = (root / args.output).resolve()
profile = out / "profile"
assert not profile.exists(), "Use a fresh profile"
out.mkdir(parents=True, exist_ok=True)
report = {"passed": False, "checks": [], "music_transition": []}
process = None

def check(condition, label):
    assert condition, label
    report["checks"].append(label)
    print("PASS " + label, flush=True)

def command(op, **fields):
    result = client.command(op, **fields)
    assert result["ok"], (op, result)
    return result

def wait(predicate, timeout=120):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        state = command("state")
        if predicate(state):
            return state
        time.sleep(.15)
    raise AssertionError("State timeout: " + str(state))

def actors(name, parts=False):
    return command("actors", filter=name, parts=parts)["actors"]

def shot(name):
    time.sleep(.8)
    command("shot", name=name)
    time.sleep(.3)

def music(state, name):
    return next((track for track in state["music"] if track["name"] == name and track["playing"]), None)

try:
    for platform in ("Windows", "WindowsEditor"):
        config = profile / "Saved/Config" / platform / "GameUserSettings.ini"
        config.parent.mkdir(parents=True, exist_ok=True)
        config.write_text("[BrainMining.Localization]\nLanguagePreference=zh-Hans\n", encoding="utf8")
    launch = [str(Path(args.exe).resolve())] if args.exe else [
        "C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe", str(root / "BrainMining.uproject"),
        "/Game/MainGame/Maps/Lvl_MainGame", "-game", "-EnablePlugins=GameFeatures",
        "-DisablePlugins=AllToolsets,ModelContextProtocol"]
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen(launch + ["-windowed", "-ResX=1280", "-ResY=720", "-RenderOffscreen",
        "-NoSplash", "-unattended", "-MiningQA", "-UserDir=" + str(profile), "-abslog=" + str(out / "game.log")], startupinfo=startup)
    client = Client(profile)
    client.command("state", timeout=180)
    wait(lambda state: bool(state["menu_buttons"]))
    command("start")
    wait(lambda state: not state["opening"])
    command("gm_tool", tool=0)
    command("area_fixture", area=3)
    time.sleep(2)
    stage = actors("CardRoomStage")[0]
    p = stage["position"]
    command("pose", eye=[p[0]-75, p[1], p[2]+115], target=p)
    wait(lambda state: state["pickaxe_visible"])
    check(True, "Pickaxe is visible before taking the card seat")
    command("card", action="interact")
    wait(lambda state: state["card_phase"] == 4)
    command("card", action="leave")
    wait(lambda state: state["pickaxe_visible"], timeout=10)
    check(True, "Leaving an unfinished card encounter restores the pickaxe")
    command("card", action="interact")
    wait(lambda state: state["card_phase"] == 4)
    for round_number in range(1, 5):
        state = command("state")
        check(state["card_round"] == round_number and not state["pickaxe_visible"], "Pickaxe hidden while choosing round " + str(round_number))
        if round_number == 1:
            shot("cards-choosing")
        command("card", action="all")
        command("card", action="confirm")
        state = wait(lambda state: state["card_phase"] == 5)
        check(not state["pickaxe_visible"], "Pickaxe hidden during card attacks " + str(round_number))
        if round_number == 1:
            shot("cards-resolving")
        wait(lambda state: state["card_phase"] in (4, 6))
    check(not command("state")["pickaxe_visible"], "Pickaxe stays hidden at victory until leaving the seat")
    command("card", action="interact")
    # Victory awards and equips the drill; both models must become visible.
    wait(lambda state: not state["move_ignored"] and "DrillRightModel" in state["visible_tools"] and "DrillLeftModel" in state["visible_tools"], timeout=10)
    check(True, "Leaving the winning card seat restores the awarded drill")

    for name, eye, target in [
        ("stage-ceiling", [31800, 11750, 20250], [32150, 11750, 21000]),
        ("stage-exit-wide", [32100, 11750, 20100], [33200, 11750, 20050]),
        ("stage-door-close", [32580, 11750, 20060], [33200, 11750, 20050]),
        ("stage-cave-inside", [32850, 11750, 20060], [33200, 11750, 20050])]:
        command("pose", eye=eye, target=target)
        shot(name)

    command("area_fixture", area=4)
    client.key("C")
    command("catalogue_owned_fixture", item=0)
    command("catalogue_rotate", item=0, yaw=0, pitch=0)
    wait(lambda state: state["catalogue_preview_visible"])
    previews = [actor for actor in actors("HeavenToilet", True) if actor["position"][2] < -9000]
    check(any(part["name"] == "SprayDuck" and part["visible"] and part["mesh"] for actor in previews for part in actor["parts"]), "Catalogue toilet contains a visible duck mesh")
    shot("toilet-duck-front")
    command("catalogue_rotate", item=0, yaw=90, pitch=25)
    shot("toilet-duck-rotated")
    client.key("C")

    command("area_fixture", area=5)
    museum_name = "M_Room_Museum_Loop"
    ending_name = "M_EE_LightThought_Loop"
    state = wait(lambda state: music(state, museum_name) and music(state, museum_name)["gain"] > .2)
    original = music(state, museum_name)
    check(original["persistent"], "Museum music can survive level travel")
    command("mouse_gallery_fixture")
    portal = actors("MouseMuseumPortal")[0]
    cell = portal["cell"]
    eye = [cell[0]-90, cell[1], cell[2]]
    command("pose", eye=eye, target=cell)
    actual = command("state")["camera"]
    command("pose", eye=[2*eye[i]-actual[i] for i in range(3)], target=cell)
    time.sleep(.5)
    command("aim", target=cell)
    check(actors("MouseMuseumPortal")[0]["focused"], "Actual museum circle is focused")
    client.key("E")
    deadline = time.monotonic() + 100
    saw_overlap = False
    while time.monotonic() < deadline:
        state = command("state", timeout=180)
        report["music_transition"].append({"time": time.monotonic(), "ending": state.get("ending_gate"), "music": state["music"]})
        old, new = music(state, museum_name), music(state, ending_name)
        if old:
            check_id = old["player_id"] == original["player_id"]
            assert check_id, "Museum track restarted during portal transition"
        if "ending_gate" in state and old and new and old["fading_out"]:
            saw_overlap = True
        if saw_overlap and not old and new and new["gain"] >= .44:
            break
        time.sleep(.1)
    check(saw_overlap, "Same museum music voice overlaps and fades into ending music after real portal travel")
    check(not old and new and new["gain"] >= .44, "Museum music is released after the ending reaches normal gain")
    shot("ending-after-music-transition")
    report["passed"] = True
except Exception:
    report["error"] = traceback.format_exc()
    print(report["error"], flush=True)
finally:
    if process and process.poll() is None:
        try:
            command("quit")
            process.wait(timeout=60)
        except Exception:
            process.terminate()
            process.wait(timeout=15)
    report["exit_code"] = process.returncode if process else None
    (out / "runtime.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf8")
if not report["passed"]:
    raise SystemExit(1)
