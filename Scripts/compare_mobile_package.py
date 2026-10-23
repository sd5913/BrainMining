import argparse
import collections
import csv
import json
import zipfile
from pathlib import Path


def read_chunks(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream, skipinitialspace=True))


def asset_sizes(chunks):
    assets = collections.Counter()
    for chunk in chunks:
        filename = chunk["Filename"]
        for suffix in (".m.ubulk", ".uasset", ".umap", ".ubulk", ".uptnl"):
            if filename.endswith(suffix):
                filename = filename[:-len(suffix)]
                break
        assets[filename] += int(chunk["CompressedSize"])
    return assets


def folder_sizes(chunks):
    folders = collections.Counter()
    for chunk in chunks:
        filename = chunk["Filename"]
        if filename.startswith("../../../BrainMining/Content/"):
            category = "Game/" + filename.removeprefix("../../../BrainMining/Content/").split("/", 1)[0]
        elif filename.startswith("../../../Engine/Plugins/"):
            category = "EnginePlugin/" + filename.split("/Content/", 1)[0].split("/")[-1]
        elif filename.startswith("../../../Engine/Content/"):
            category = "Engine/" + filename.removeprefix("../../../Engine/Content/").split("/", 1)[0]
        else:
            category = filename
        folders[category] += int(chunk["CompressedSize"])
    return folders


parser = argparse.ArgumentParser()
parser.add_argument("--baseline", type=Path, required=True)
parser.add_argument("--run", type=Path, required=True)
options = parser.parse_args()
root = Path(__file__).resolve().parents[1]
baseline = json.loads((options.baseline / "summary.json").read_text(encoding="utf-8-sig"))
result = json.loads((options.run / "result.json").read_text(encoding="utf-8-sig"))
changes = json.loads((root / "Saved/Validation/SizeOptimization/content_changes.json").read_text(encoding="utf-8-sig"))
request = json.loads((options.run / "request.json").read_text(encoding="utf-8-sig"))
old_chunks = read_chunks(options.baseline / "android_iostore.csv")
new_chunks = read_chunks(options.run / "android_iostore.csv")
old_assets, new_assets = asset_sizes(old_chunks), asset_sizes(new_chunks)
baseline_names = {path.casefold(): path for path in old_assets}
new_assets = collections.Counter({baseline_names.get(path.casefold(), path): size for path, size in new_assets.items()})
old_folders, new_folders = folder_sizes(old_chunks), folder_sizes(new_chunks)
required = [path.replace("/Game/", "../../../BrainMining/Content/", 1) for path in request.get("native_materials", changes["native_materials"])]
required += ["../../../BrainMining/Content/MainGame/Maps/Lvl_MainGame", "../../../BrainMining/Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo"]
required += [path for path in old_assets if "/Fonts/" in path or "/UI/" in path]
cell = "../../../BrainMining/Content/Fab/Cell/Cell/StaticMeshes/cell"
removed = sorted(set(old_assets) - set(new_assets))
checks = {
    "required_assets_present": all(path.casefold() in {name.casefold() for name in new_assets} for path in required),
    "default_hdri_absent": not any("/HDRIBackdrop/" in path for path in new_assets),
    "old_cloud_absent": not any(path.endswith("/T_HeavenClouds") for path in new_assets),
    "cell_smaller": 0 < new_assets[cell] < old_assets[cell],
    "apk_smaller": result["bytes"] < baseline["apk"]["bytes"],
}
container_checks = {}
with zipfile.ZipFile(result["apk"]) as apk:
    entries = [{"path": item.filename, "bytes": item.compress_size, "uncompressed_bytes": item.file_size} for item in apk.infolist()]
    with apk.open("assets/main.obb.png") as obb_file:
        with zipfile.ZipFile(obb_file) as obb:
            inner_entries = [{"path": item.filename, "bytes": item.file_size} for item in obb.infolist()]
            for item in obb.infolist():
                if item.filename.endswith(".utoc"):
                    matches = list((options.run / "Staged").rglob(Path(item.filename).name))
                    container_checks[item.filename] = len(matches) == 1 and matches[0].read_bytes() == obb.read(item.filename)
checks["staged_containers_match_apk"] = bool(container_checks) and all(container_checks.values())
report = {
    "passed": all(checks.values()), "checks": checks,
    "baseline_apk_bytes": baseline["apk"]["bytes"], "optimized_apk_bytes": result["bytes"],
    "saved_bytes": baseline["apk"]["bytes"] - result["bytes"],
    "reduction_percent": 100 * (1 - result["bytes"] / baseline["apk"]["bytes"]),
    "cell_before_bytes": old_assets[cell], "cell_after_bytes": new_assets[cell],
    "missing_required_assets": sorted(path for path in set(required) if path.casefold() not in {name.casefold() for name in new_assets}),
    "removed_assets": removed, "added_assets": sorted(set(new_assets) - set(old_assets)),
    "container_matches": container_checks, "apk_entries": entries, "obb_entries": inner_entries,
    "folders": [{"path": path, "before_bytes": old_folders[path], "after_bytes": new_folders[path], "saved_bytes": old_folders[path] - new_folders[path]} for path in sorted(set(old_folders) | set(new_folders), key=lambda path: old_folders[path] - new_folders[path], reverse=True)],
}
(options.run / "comparison.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
with (options.run / "asset_comparison.csv").open("w", encoding="utf-8-sig", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(["path", "before_bytes", "after_bytes", "saved_bytes"])
    for path in sorted(set(old_assets) | set(new_assets), key=lambda path: old_assets[path] - new_assets[path], reverse=True):
        writer.writerow([path, old_assets[path], new_assets[path], old_assets[path] - new_assets[path]])
print(json.dumps({key: report[key] for key in ("passed", "checks", "optimized_apk_bytes", "saved_bytes", "reduction_percent", "cell_before_bytes", "cell_after_bytes", "missing_required_assets")}, indent=2))
raise SystemExit(0 if report["passed"] else 1)
