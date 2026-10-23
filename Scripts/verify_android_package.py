import argparse
import csv
import hashlib
import json
import zipfile
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("--apk", type=Path, required=True)
parser.add_argument("--stage", type=Path, required=True)
parser.add_argument("--report", type=Path, required=True)
parser.add_argument("--evidence", type=Path, required=True)
parser.add_argument("--iostore-csv", type=Path)
parser.add_argument("--required-json", type=Path)
options = parser.parse_args()
checks = {}
with zipfile.ZipFile(options.apk) as apk:
    with apk.open("assets/main.obb.png") as obb_file:
        with zipfile.ZipFile(obb_file) as obb:
            for entry in obb.infolist():
                if entry.filename.endswith((".utoc", ".ucas", ".pak")):
                    staged = options.stage / entry.filename
                    checks[entry.filename] = staged.is_file() and staged.stat().st_size == entry.file_size
                    if checks[entry.filename]:
                        with staged.open("rb") as stream:
                            stage_hash = hashlib.file_digest(stream, "sha256").hexdigest()
                        with obb.open(entry.filename) as stream:
                            package_hash = hashlib.file_digest(stream, "sha256").hexdigest()
                        checks[entry.filename] = stage_hash == package_hash
                    if checks[entry.filename] and entry.filename.endswith(".utoc"):
                        evidence = options.evidence / entry.filename
                        evidence.parent.mkdir(parents=True, exist_ok=True)
                        evidence.write_bytes(staged.read_bytes())
if options.iostore_csv and options.required_json:
    request = json.loads(options.required_json.read_text(encoding="utf-8-sig"))
    with options.iostore_csv.open(encoding="utf-8-sig", newline="") as stream:
        filenames = {row["Filename"].casefold() for row in csv.DictReader(stream, skipinitialspace=True)}
    for asset in request["native_materials"]:
        filename = asset.replace("/Game/", "../../../BrainMining/Content/", 1) + ".uasset"
        checks[asset] = filename.casefold() in filenames
    for asset in ("MainGame/Maps/Lvl_MainGame", "EndingEmergence/Maps/Lvl_EndingEmergence_Demo"):
        checks[asset] = ("../../../BrainMining/Content/" + asset + ".umap").casefold() in filenames
    checks["default_hdri_absent"] = not any("/hdribackdrop/" in filename for filename in filenames)
    checks["old_cloud_absent"] = not any("/t_heavenclouds." in filename for filename in filenames)
passed = bool(checks) and any(name.endswith(".ucas") for name in checks) and all(checks.values())
options.report.write_text(json.dumps({"passed": passed, "apk": str(options.apk), "checks": checks}, indent=2), encoding="utf-8")
print(json.dumps({"passed": passed, "checks": checks}, indent=2))
raise SystemExit(0 if passed else 1)
