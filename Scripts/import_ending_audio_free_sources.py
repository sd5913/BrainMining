import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "SourceAssets/EndingEmergence/AudioFreeSources"
entries = json.loads((SOURCE / "manifest.json").read_text(encoding="utf-8"))
classes = {name: unreal.load_asset("/Game/Audio/Mix/SC_" + name) for name in ("Music", "Effects", "Ambience")}
if not all(classes.values()):
    raise RuntimeError("Audio mix classes are missing; run import_audio_pilot.py first")

results = []
for entry in entries:
    filename = SOURCE / entry["import_file"]
    if hashlib.sha256(filename.read_bytes()).hexdigest() != entry["sha256"]:
        raise RuntimeError("Processed audio hash mismatch: " + str(filename))
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = "/Game/EndingEmergence/Audio"
    task.destination_name = entry["asset"]
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = task.destination_path + "/" + task.destination_name
    sound = unreal.load_asset(path)
    if not isinstance(sound, unreal.SoundWave) or not task.imported_object_paths:
        raise RuntimeError("Import failed: " + path)
    sound.set_editor_property("looping", entry["loop"])
    sound.set_editor_property("volume", 1.0)
    sound.set_editor_property("sound_class_object", classes[entry["category"]])
    sound.set_editor_property("virtualization_mode", unreal.VirtualizationMode.PLAY_WHEN_SILENT)
    if entry["loop"]:
        sound.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD)
    concurrency = unreal.SoundConcurrencySettings()
    concurrency.set_editor_property("max_count", 2 if entry["loop"] else 4)
    concurrency.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
    concurrency.set_editor_property("volume_scale", 0.7 if entry["loop"] else 1.0)
    sound.set_editor_property("override_concurrency", True)
    sound.set_editor_property("concurrency_overrides", concurrency)
    if sound.get_editor_property("num_channels") != entry["channels"]:
        raise RuntimeError("Channel mismatch: " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + path)
    results.append(dict(asset=path, category=entry["category"], loop=entry["loop"], channels=entry["channels"]))

report = ROOT / "Saved/EndingEmergenceAudioImport.json"
report.write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("ENDING_EMERGENCE_AUDIO_IMPORT_OK: " + str(len(results)))
