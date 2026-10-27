import json
import os
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir())
REPORT = ROOT / "Saved/Validation/LocalizationAchievements/subtitle-assets.json"
CATALOGUE = ROOT / "SourceAssets/Localization/Subtitles.json"
MODE = os.environ.get("BM_LOCALIZATION_MODE", "audit")
assert MODE in ("audit", "apply", "verify"), MODE
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), "Stop PIE before migrating text assets"
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
assets = registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/BrainMining", "SubtitleSegment"), True)
entries = json.loads(CATALOGUE.read_text(encoding="utf-8")) if CATALOGUE.exists() else {}
rows = []
changes = []
errors = []
excluded_assets = []
for asset in sorted(assets, key=lambda item: str(item.package_name)):
    segment = asset.get_asset()
    lines = list(segment.get_editor_property("lines"))
    package = str(asset.package_name)
    if package.startswith("/Game/Showcase/Subtitles/"):
        excluded_assets.append(package)
        continue
    if not package.startswith("/Game/Subtitles/"):
        errors.append("Subtitle outside the reviewed folder: " + package)
        continue
    if str(asset.asset_name) in ("DA_aaaatest", "DA_0002"):
        continue
    for index, line in enumerate(lines):
        key = "Subtitle." + str(asset.asset_name).removeprefix("DA_") + f".{index + 1:03d}"
        text = line.get_editor_property("text")
        source = unreal.TextLibrary.get_text_source_string(text)
        rows.append({"asset": package, "segment_id": str(segment.segment_id), "line": index, "key": key, "source": source,
                     "from_string_table": unreal.TextLibrary.text_is_from_string_table(text),
                     "audio": str(line.get_editor_property("audio"))})
        if MODE in ("apply", "verify"):
            if key not in entries or entries[key]["zh-Hans"] != source:
                errors.append("Missing or stale translation: " + key)
                continue
            expected = unreal.TextLibrary.text_from_string_table("BrainMining.Game", key)
            if unreal.TextLibrary.get_text_source_string(expected) != source:
                errors.append("Regenerate the string table before migrating: " + key)
                continue
            if MODE == "verify":
                actual = unreal.TextLibrary.string_table_id_and_key_from_text(text)
                if not actual or str(actual[0]) != "BrainMining.Game" or str(actual[1]) != key:
                    errors.append("Missing stable text reference: " + key)
            else:
                line.set_editor_property("text", expected)
    if MODE == "apply":
        changes.append((segment, lines))
REPORT.parent.mkdir(parents=True, exist_ok=True)
REPORT.write_text(json.dumps({"mode": MODE, "lines": rows, "errors": errors, "excluded_demo_assets": excluded_assets}, ensure_ascii=False, indent=2), encoding="utf-8")
assert not errors, "\n".join(errors)
for segment, lines in changes:
    segment.set_editor_property("lines", lines)
    assert unreal.EditorAssetLibrary.save_loaded_asset(segment), segment.get_path_name()
unreal.log(f"LOCALIZATION_SUBTITLES_{MODE.upper()} {len(rows)} lines; {len(changes)} assets saved")
