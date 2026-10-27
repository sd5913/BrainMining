import argparse
import csv
import io
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "Config/Custom/SteamPC/DefaultEngine.ini"


def definitions():
    source = (ROOT / "Source/BrainMining/MiningAchievementState.cpp").read_text(encoding="utf-8")
    rows = re.findall(r'\{TEXT\("(ACH_[A-Z0-9_]+)"\), TEXT\("(Achievement\.[A-Za-z0-9]+)"\)\}', source)
    if len(rows) != 8 or len({row[0] for row in rows}) != len(rows):
        raise ValueError("Review the release achievement list before exporting a changed schema")
    catalogue = {}
    for path in sorted((ROOT / "SourceAssets/Localization").glob("*.json")):
        catalogue.update(json.loads(path.read_text(encoding="utf-8")))
    result = []
    for identity, prefix in rows:
        result.append({"api_name": identity, "hidden": False, "stat": None,
                       "languages": {steam: {"name": catalogue[prefix + ".Name"][culture],
                                             "description": catalogue[prefix + ".Description"][culture]}
                                     for steam, culture in (("schinese", "zh-Hans"), ("english", "en"), ("tchinese", "zh-Hant"))}})
    return result


def steam_config(app_id, entries):
    return ("[OnlineSubsystem]\nDefaultPlatformService=Steam\n\n[OnlineSubsystemSteam]\n"
            "bEnabled=True\nbRelaunchInSteam=True\nSteamDevAppId=" + str(app_id) + "\n" +
            "\n".join(f'Achievement_{index}_Id="{entry["api_name"]}"' for index, entry in enumerate(entries)) +
            '\nAchievement_' + str(len(entries)) + '_Id=""\n\n[BrainMining.Steam]\nExpectedAppId=' + str(app_id) + '\n')


def main():
    parser = argparse.ArgumentParser(description="Export reviewable Steam achievement definitions; configure a real AppID when available")
    parser.add_argument("--app-id", type=int)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if args.app_id is not None and (args.check or args.app_id <= 0 or args.app_id == 480 or args.app_id > 2147483647):
        parser.error("Supply your own positive AppID (not 480); --check cannot change it")
    entries = definitions()
    outputs = {ROOT / "Config/SteamAchievementDefinitions.json": json.dumps(entries, ensure_ascii=False, indent=2) + "\n"}
    table = io.StringIO(newline="")
    writer = csv.writer(table, lineterminator="\n")
    writer.writerow(("api_name", "language", "name", "description", "hidden"))
    for entry in entries:
        for culture, text in entry["languages"].items():
            writer.writerow((entry["api_name"], culture, text["name"], text["description"], "0"))
    outputs[ROOT / "Docs/SteamAchievementDefinitions.csv"] = table.getvalue()
    if args.app_id is not None:
        outputs[CONFIG] = steam_config(args.app_id, entries)
    elif not CONFIG.exists():
        outputs[CONFIG] = steam_config(0, entries)
    else:
        current = CONFIG.read_text(encoding="utf-8")
        match = re.search(r"(?m)^ExpectedAppId=(\d+)$", current)
        if not match or current != steam_config(int(match[1]), entries):
            raise ValueError("Steam config differs from the canonical schema; reconfigure with --app-id")
    for path, content in outputs.items():
        if args.check:
            if not path.exists() or path.read_text(encoding="utf-8") != content:
                raise ValueError(f"Stale Steam artifact: {path}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8", newline="\n")
    print(f"Validated {len(entries)} Steam definitions in three languages. No backend changes were made.")


if __name__ == "__main__":
    main()
