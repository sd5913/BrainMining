import json
from datetime import datetime
from pathlib import Path

root = Path(__file__).resolve().parents[1]
build_root = root / "Builds/MobileVerticalSlice"
report = root / "Saved/Validation/SizeOptimization"
report.mkdir(parents=True, exist_ok=True)
builds = sorted((path for path in build_root.iterdir() if path.is_dir()), key=lambda path: path.stat().st_mtime, reverse=True)
keep = {path.name for path in builds[:2]} | {"Android", "AndroidMainGameV15", "AndroidMainGameV29", "AndroidTapPlayCompatibility", "AndroidRemoteSync20261021", "AndroidRemoteSync20261021v112"}
allowed = {".apk", ".obb", ".so", ".bat", ".cmd", ".txt", ".json", ".zip", ".aab", ".log", ".sig", ".manifest", ".exe", ".png"}
candidates = []
for build in builds:
    files = [path for path in build.rglob("*") if path.is_file()]
    if build.name not in keep and any(path.suffix == ".apk" for path in files) and all(path.suffix.lower() in allowed for path in files):
        candidates.append({"path": str(build), "reason": "Generated build outside retained versions", "bytes": sum(path.stat().st_size for path in files)})
for run in (root / "Saved/Validation/Gamepad").iterdir():
    cache = run / "User/Intermediate/CachedAssetRegistry"
    if cache.is_dir():
        candidates.append({"path": str(cache), "reason": "Regenerable test asset index; reports preserved", "bytes": sum(path.stat().st_size for path in cache.rglob("*") if path.is_file())})
result = {"created": datetime.now().isoformat(), "read_only": True, "kept_builds": sorted(keep), "candidate_bytes": sum(item["bytes"] for item in candidates), "candidates": candidates}
(report / "storage_plan.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"candidate_gib": result["candidate_bytes"] / 2**30, "directories": len(candidates), "kept_builds": sorted(keep)}, ensure_ascii=False, indent=2))
