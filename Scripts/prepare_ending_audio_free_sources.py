import hashlib
import json
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Saved" / "AudioPilotTools"))
import soundfile as sf


SOURCE_ROOT = ROOT / "SourceAssets" / "EndingEmergence" / "AudioFreeSources"
ORIGINALS = SOURCE_ROOT / "Originals"
WAV_ROOT = SOURCE_ROOT / "Wav"
OUTPUT_ROOT = ROOT / "SourceAssets" / "EndingEmergence" / "Audio"
RATE = 48000
RECORDS = []


def read_source(relative_path, gain=1.0, start=0.0, end=None, mono=False):
    path = ORIGINALS / relative_path
    samples, rate = sf.read(path, dtype="float32", always_2d=True)
    first = max(0, round(start * rate))
    last = len(samples) if end is None else min(len(samples), round(end * rate))
    samples = samples[first:last]
    if mono:
        samples = samples.mean(axis=1, keepdims=True)
    if rate != RATE:
        source_x = np.arange(len(samples), dtype=np.float64)
        target_len = max(2, round(len(samples) * RATE / rate))
        target_x = np.arange(target_len, dtype=np.float64) * rate / RATE
        resampled = np.empty((target_len, samples.shape[1]), dtype=np.float32)
        for channel in range(samples.shape[1]):
            resampled[:, channel] = np.interp(target_x, source_x, samples[:, channel])
        samples = resampled
    samples = samples.astype(np.float64) * gain
    return samples, dict(
        file=str(path.relative_to(ROOT)).replace("\\", "/"),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        gain=gain,
        start=start,
        end=end,
        mono=mono,
        source_rate=rate,
    )


def render(name, sources, category, loop=False, target_peak=0.4, crossfade=0.12, duration=None):
    prepared = []
    channels = max(samples.shape[1] for samples, _, _ in sources)
    for samples, record, offset in sources:
        if samples.shape[1] == 1 and channels > 1:
            samples = np.repeat(samples, channels, axis=1)
        if samples.shape[1] != channels:
            raise ValueError(f"channel mismatch for {name}")
        prepared.append((samples, record, round(offset * RATE)))
    length = round(duration * RATE) if duration else max(len(samples) + offset for samples, _, offset in prepared)
    mixed = np.zeros((max(length, 4800), channels), dtype=np.float64)
    for samples, _, offset in prepared:
        if offset >= len(mixed):
            continue
        count = min(len(samples), len(mixed) - offset)
        mixed[offset:offset + count] += samples[:count]
    mixed -= mixed.mean(axis=0, keepdims=True)
    if loop:
        overlap = min(round(crossfade * RATE), len(mixed) // 8)
        if overlap > 1:
            blend = np.linspace(0.0, 1.0, overlap)[:, None]
            seam = mixed[-overlap:] * (1.0 - blend) + mixed[:overlap] * blend
            mixed = np.concatenate((mixed[overlap:-overlap], seam))
    else:
        fade = min(round(0.012 * RATE), len(mixed) // 8)
        if fade > 1:
            mixed[:fade] *= np.linspace(0.0, 1.0, fade)[:, None]
            mixed[-fade:] *= np.linspace(1.0, 0.0, fade)[:, None]
    peak = float(np.max(np.abs(mixed)))
    if not np.isfinite(mixed).all() or peak <= 1e-7:
        raise ValueError(f"invalid rendered audio: {name}")
    mixed *= target_peak / peak
    destination = WAV_ROOT / f"{name}.wav"
    destination.parent.mkdir(parents=True, exist_ok=True)
    sf.write(destination, mixed, RATE, subtype="PCM_16")
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    output = OUTPUT_ROOT / destination.name
    output.write_bytes(destination.read_bytes())
    RECORDS.append(
        dict(
            asset=name,
            file=str(output.relative_to(ROOT)).replace("\\", "/"),
            import_file=str(destination.relative_to(SOURCE_ROOT)).replace("\\", "/"),
            category=category,
            loop=loop,
            channels=channels,
            sample_rate=RATE,
            seconds=round(len(mixed) / RATE, 4),
            peak=float(np.max(np.abs(mixed))),
            rms=float(np.sqrt(np.mean(mixed * mixed))),
            sources=[dict(record, offset=offset) for _, record, offset in sources],
            processing=dict(resample_to=RATE, crossfade_seconds=crossfade if loop else 0, target_peak=target_peak),
            sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        )
    )


def one(name, source, category, loop=False, target_peak=0.4, crossfade=0.12, duration=None, gain=1.0, mono=False):
    samples, record = read_source(source, gain=gain, mono=mono)
    render(name, [(samples, record, 0.0)], category, loop, target_peak, crossfade, duration)


def main():
    WAV_ROOT.mkdir(parents=True, exist_ok=True)
    one("M_EE_Main_Loop", "DST-MindStream.mp3", "Music", loop=True, target_peak=0.30, crossfade=3.0, duration=120.0)
    one("M_EE_Cosmic_Loop", "S31-Tunnel-Drilling.ogg", "Music", loop=True, target_peak=0.24, crossfade=2.0, duration=84.0)
    one("M_EE_Inner_Loop", "EndingAmbiencePack/The_Surreal_Truth.mp3", "Music", loop=True, target_peak=0.20, crossfade=2.0)
    one("M_EE_Drill_Loop", "EndingAmbiencePack/Infestation_in_the_Control_Room.mp3", "Music", loop=True, target_peak=0.16, crossfade=2.0)
    one("M_EE_Resolve_Loop", "EndingAmbiencePack/Final_Captains_Log.mp3", "Music", loop=True, target_peak=0.22, crossfade=2.5)
    one("A_EE_Cell_Loop", "deep-humidity.ogg", "Ambience", loop=True, target_peak=0.22, crossfade=0.3, duration=55.0, mono=True)
    one("A_EE_Blood_Loop", "dungeon002.ogg", "Ambience", loop=True, target_peak=0.18, crossfade=0.3, duration=48.0, mono=True)
    one("A_EE_Drill_Loop", "Kenney/engineCircular_000.ogg", "Ambience", loop=True, target_peak=0.30, crossfade=0.08, duration=2.2, mono=True)
    one("S_EE_ScaleWhoosh_01", "Kenney/forceField_000.ogg", "Effects", target_peak=0.52, mono=True)
    one("S_EE_ScaleWhoosh_02", "Kenney/spaceEngine_000.ogg", "Effects", target_peak=0.48, mono=True)
    one("S_EE_NeuralPulse_01", "Kenney/laserSmall_000.ogg", "Effects", target_peak=0.42, mono=True)
    one("S_EE_DrillHit_01", "Kenney/impactMetal_000.ogg", "Effects", target_peak=0.48, mono=True)
    one("S_EE_DrillDebris_01", "Kenney/explosionCrunch_000.ogg", "Effects", target_peak=0.42, mono=True)
    one("S_EE_PaperFlight", "paper_sound_-_1.mp3", "Effects", target_peak=0.30, mono=True)
    one("S_EE_PaperLand", "paper_crushed_-_1.mp3", "Effects", target_peak=0.34, mono=True)
    one("S_EE_FinalResolve", "snd_close_map.wav", "Effects", target_peak=0.38, mono=True)
    samples, first = read_source("Kenney/computerNoise_000.ogg", gain=0.55, mono=True)
    samples2, second = read_source("snd_close_map.wav", gain=0.20, start=0.12, mono=True)
    render("S_EE_FinalComputer", [(samples, first, 0.0), (samples2, second, 0.35)], "Effects", target_peak=0.38)
    manifest = SOURCE_ROOT / "manifest.json"
    manifest.write_text(json.dumps(RECORDS, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    credits = [
        "Ending Emergence free audio selection — 2026-10-18",
        "Processing: resampled to 48 kHz PCM16, normalized, trimmed where needed, and crossfaded for loops.",
        "All source files are retained under AudioFreeSources/Originals for auditability.",
        "",
        "OpenGameArt sources:",
        "MindStream — DST — CC0 — https://opengameart.org/content/mindstream",
        "Deep Humidity — yd — CC0 — https://opengameart.org/content/deep-humidity",
        "Loopable Dungeon Ambience — remaxim — CC0 — https://opengameart.org/content/loopable-dungeon-ambience",
        "S31 Tunnel Drilling — yd — CC0 — https://opengameart.org/content/s31-tunnel-drilling",
        "Ambience Pack 1: Sci-Fi Horror — remaxim — CC0 — https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "The Surreal Truth, Infestation in the Control Room, Final Captain's Log — remaxim — CC0 — selected from the Ambience Pack 1 source page",
        "Paper sounds — OpenGameArt CC0 assets — retained with source filenames",
        "",
        "Kenney Sci-Fi Sounds — Kenney — CC0 — https://kenney.nl/assets/sci-fi-sounds",
        "Map/paper sounds are retained as their original OpenGameArt files and are only used as short effects.",
    ]
    (SOURCE_ROOT / "CREDITS.txt").write_text("\n".join(credits) + "\n", encoding="utf-8")
    print(json.dumps({"assets": len(RECORDS), "seconds": round(sum(item["seconds"] for item in RECORDS), 2)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
