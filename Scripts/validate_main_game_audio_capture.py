import argparse
import array
import json
import math
import sys
import wave
from pathlib import Path


def validate(saved):
    runtime = json.loads((saved / "MainGameAudioRuntime.json").read_text(encoding="utf-8-sig"))
    with wave.open(str(saved / "MainGameAudioRuntime.wav"), "rb") as recording:
        if recording.getsampwidth() != 2 or recording.getcomptype() != "NONE":
            raise ValueError("Expected uncompressed PCM16 engine recording")
        channels = recording.getnchannels()
        rate = recording.getframerate()
        duration = recording.getnframes() / rate
        samples = array.array("h", recording.readframes(recording.getnframes()))
    if sys.byteorder != "little":
        samples.byteswap()
    events = {event["step"]: event["audio_time"] for event in runtime["events"]}
    if set(events) != set(range(27)):
        raise ValueError("Incomplete MainGame capture events")

    def window(start, end):
        if not 0 <= start < end <= duration:
            raise ValueError(f"Invalid recording window: {start} to {end}")
        segment = samples[int(start * rate) * channels:int(end * rate) * channels]
        return {
            "start_seconds": start,
            "end_seconds": end,
            "peak": max(abs(sample) for sample in segment) / 32768,
            "rms": math.sqrt(sum(sample * sample for sample in segment) / len(segment)) / 32768,
        }

    mute = window(events[15] + 0.4, events[16] - 0.2)
    recovery = window(events[16] + 0.4, events[17] - 0.2)
    before_mute = window(events[14] + 0.4, events[15] - 0.2)
    isolated_music = {
        name: window(events[step] + 0.4, events[step + 1] - 0.2)
        for name, step in [("cavern", 19), ("stalagmites", 21), ("neural_rock", 23)]
    }
    music_mute = window(events[24] + 0.4, events[25] - 0.2)
    music_recovery = window(events[25] + 0.4, events[26] - 0.2)
    peak = max(abs(sample) for sample in samples) / 32768
    clipped = sum(sample in (-32768, 32767) for sample in samples)
    checks = {
        "correct_map": runtime.get("map") == "/Game/MainGame/Maps/Lvl_MainGame",
        "runtime_checks_passed": runtime["passed"] and runtime["failures"] == 0 and runtime["checks"] >= 32,
        "stereo_48khz": channels == 2 and rate == 48000,
        "recording_nonempty": duration > 30 and peak > 0.001,
        "no_clipping": clipped == 0,
        "audio_clock_matches_recording": abs(duration - events[26]) < 0.25,
        "audible_before_mute": before_mute["rms"] > 0.0001,
        "master_mute_silent": mute["peak"] <= 1 / 32768,
        "audio_recovers": recovery["rms"] > 0.0001,
        "later_music_audible": all(segment["rms"] >= 0.008 for segment in isolated_music.values()),
        "music_slider_mutes": music_mute["peak"] <= 1 / 32768,
        "music_slider_recovers": music_recovery["rms"] >= 0.008,
    }
    report = {
        "passed": all(checks.values()), "checks": checks, "duration_seconds": duration,
        "sample_rate": rate, "channels": channels, "peak": peak,
        "clipped_samples": clipped, "before_mute": before_mute,
        "master_mute": mute, "recovery": recovery,
        "isolated_music": isolated_music, "music_mute": music_mute, "music_recovery": music_recovery,
    }
    (saved / "MainGameAudioWaveformValidation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return report["passed"]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Validate actual MainGame recording using the audio device clock.")
    parser.add_argument("--saved", type=Path, default=Path(__file__).resolve().parents[1] / "Saved")
    arguments = parser.parse_args()
    raise SystemExit(0 if validate(arguments.saved) else 1)
