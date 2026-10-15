import hashlib
import io
import json
import pathlib
import sys
import zipfile

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved' / 'AudioPilotTools'))
import soundfile as sf

CACHE = ROOT / 'Saved' / 'AudioPilotDownloads'
OUTPUT = ROOT / 'SourceAssets' / 'AudioPilot'
RATE = 48000
MANIFEST = []
SOURCES = {
    'impact': ('Kenney', 'https://kenney.nl/assets/impact-sounds', 'CC0'),
    'rpg': ('Kenney', 'https://kenney.nl/assets/rpg-audio', 'CC0'),
    'interface': ('Kenney', 'https://kenney.nl/assets/interface-sounds', 'CC0'),
    'breaking': ('rubberduck', 'https://opengameart.org/content/75-cc0-breaking-falling-hit-sfx', 'CC0'),
    'music': ('Kevin MacLeod', 'https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100532', 'CC-BY 4.0'),
}


def read_audio(pack, filename, mono=True):
    if pack == 'music':
        raw = (CACHE / filename).read_bytes()
    else:
        with zipfile.ZipFile(CACHE / (pack + '.zip')) as archive:
            raw = archive.read(filename)
    original = OUTPUT / 'Originals' / pack / pathlib.PurePosixPath(filename).name
    original.parent.mkdir(parents=True, exist_ok=True)
    original.write_bytes(raw)
    samples, rate = sf.read(io.BytesIO(raw), dtype='float32', always_2d=True)
    if mono:
        samples = samples.mean(axis=1, keepdims=True)
    if rate != RATE:
        timeline = np.arange(round(len(samples) * RATE / rate)) * rate / RATE
        samples = np.stack([np.interp(timeline, np.arange(len(samples)), samples[:, channel]) for channel in range(samples.shape[1])], axis=1)
    return samples


def clean(samples, peak=0.65, trim=True):
    samples = samples.copy()
    samples -= samples.mean(axis=0, keepdims=True)
    if trim:
        active = np.flatnonzero(np.max(np.abs(samples), axis=1) > 0.003)
        if len(active):
            samples = samples[max(0, active[0] - 144):min(len(samples), active[-1] + 720)]
    ramp = min(240, len(samples) // 4)
    samples[:ramp] *= np.linspace(0, 1, ramp)[:, None]
    samples[-ramp:] *= np.linspace(1, 0, ramp)[:, None]
    magnitude = float(np.max(np.abs(samples)))
    if magnitude > 0:
        samples *= peak / magnitude
    return samples


def mix(*layers):
    length = max(len(samples) for samples, gain in layers)
    result = np.zeros((length, 1), dtype=np.float32)
    for samples, gain in layers:
        result[:len(samples)] += samples * gain
    return result


def save(name, samples, sources, processing, loop=False):
    target = OUTPUT / 'Wav' / (name + '.wav')
    target.parent.mkdir(parents=True, exist_ok=True)
    sf.write(target, samples, RATE, subtype='PCM_16')
    MANIFEST.append({'asset': name, 'seconds': round(len(samples) / RATE, 4), 'channels': samples.shape[1],
                     'sample_rate': RATE, 'peak': round(float(np.max(np.abs(samples))), 4), 'loop': loop,
                     'sources': sources, 'processing': processing, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})


def source(pack, filename):
    author, url, license_name = SOURCES[pack]
    return {'pack': pack, 'file': filename, 'author': author, 'url': url, 'license': license_name}


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    license_dir = OUTPUT / 'Licenses'
    license_dir.mkdir(exist_ok=True)
    for pack in ('impact', 'rpg', 'interface'):
        with zipfile.ZipFile(CACHE / (pack + '.zip')) as archive:
            (license_dir / (pack + '_License.txt')).write_bytes(archive.read('License.txt'))
    for index in range(5):
        hit_file = f'Audio/impactMining_{index:03}.ogg'
        chip_file = f'bfh1_rock_falling_{index + 1:02}.ogg'
        hit = clean(read_audio('impact', hit_file))
        chips = clean(read_audio('breaking', chip_file), peak=0.45)
        save(f'S_DigLight_{index:02}', hit, [source('impact', hit_file)], 'Mono, silence trim, 5ms fades, peak -3.7dBFS')
        save(f'S_DigDeep_{index:02}', clean(mix((hit, 1), (chips, 0.35)), peak=0.72),
             [source('impact', hit_file), source('breaking', chip_file)], 'Mining impact plus rock fragments, mono, fades, peak -2.9dBFS')
    for index in range(3):
        filename = f'bfh1_rock_breaking_{index + 1:02}.ogg'
        save(f'S_DigBreak_{index:02}', clean(read_audio('breaking', filename), peak=0.78),
             [source('breaking', filename)], 'Mono, trimmed, short fades, peak -2.2dBFS')
    for index, filename in enumerate(('Audio/knifeSlice.ogg', 'Audio/knifeSlice2.ogg', 'Audio/cloth1.ogg')):
        save(f'S_Swing_{index:02}', clean(read_audio('rpg', filename), peak=0.45),
             [source('rpg', filename)], 'Light tool swish/cloth substitute; mono, trimmed, peak -6.9dBFS')
    for material in ('concrete', 'wood'):
        for index in range(5):
            filename = f'Audio/footstep_{material}_{index:03}.ogg'
            save(f'S_Foot_{material}_{index:02}', clean(read_audio('impact', filename), peak=0.5),
                 [source('impact', filename)], 'Mono, trimmed, fades, peak -6dBFS')
    for name, filename in [('S_UI_Open', 'open_001'), ('S_UI_Close', 'close_001'), ('S_UI_Confirm', 'confirmation_001'),
                           ('S_UI_Reject', 'error_001'), ('S_Coin', 'pluck_001')]:
        filename = 'Audio/' + filename + '.ogg'
        save(name, clean(read_audio('interface', filename), peak=0.45),
             [source('interface', filename)], 'Mono, trimmed, fades, peak -6.9dBFS')
    filename = 'Audio/impactSoft_heavy_000.ogg'
    save('S_Land', clean(read_audio('impact', filename), peak=0.65), [source('impact', filename)], 'Mono soft landing, trimmed, fades')
    music = read_audio('music', 'DreamyFlashback.mp3', mono=False)
    overlap = RATE * 3
    body = music[overlap:-overlap]
    blend = np.linspace(0, 1, overlap)[:, None]
    bridge = music[-overlap:] * (1 - blend) + music[:overlap] * blend
    loop = np.concatenate((body, bridge))
    loop *= 0.75 / max(float(np.max(np.abs(loop))), 0.00001)
    save('M_DreamyFlashback_Loop', loop, [source('music', 'DreamyFlashback.mp3')],
         'Full track with 3s equal-gain tail/head overlap, stereo, peak -2.5dBFS; artistic loop transition needs in-game audition', True)
    credits = '''BrainMining audio pilot — 2026-10-15

Dreamy Flashback — Kevin MacLeod (incompetech.com)
Licensed under Creative Commons Attribution 4.0 International.
https://creativecommons.org/licenses/by/4.0/
https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1100532
Changes: decoded to PCM, resampled to 48kHz, three-second loop crossfade, gain adjustment.

Impact Sounds, RPG Audio, Interface Sounds — Kenney (https://kenney.nl)
CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/
Original pack licenses are retained beside this file.
Changes: selected excerpts, mono downmix, 48kHz PCM conversion, trimming, short fades, gain adjustment.

75 CC0 breaking / falling / hit sfx — rubberduck
https://opengameart.org/content/75-cc0-breaking-falling-hit-sfx
CC0 1.0: https://creativecommons.org/publicdomain/zero/1.0/
Changes: selected rock fragments and breaks, resampling, mono downmix, trimming and mixing with mining impacts.

Only the selected source recordings are included in Originals. Source filenames, edits and hashes are recorded in manifest.json.
'''
    (license_dir / 'CREDITS.txt').write_text(credits, encoding='utf-8')
    (OUTPUT / 'manifest.json').write_text(json.dumps(MANIFEST, ensure_ascii=False, indent=2), encoding='utf-8')
    packaged_credits = ROOT / 'Content' / 'AudioPilotCredits'
    packaged_credits.mkdir(exist_ok=True)
    (packaged_credits / 'CREDITS.txt').write_text(credits, encoding='utf-8')
    if len(MANIFEST) != 33 or any(entry['peak'] >= 1 or entry['seconds'] <= 0 for entry in MANIFEST):
        raise RuntimeError('Audio validation failed')
    print(json.dumps({'generated': len(MANIFEST), 'music_seconds': MANIFEST[-1]['seconds'], 'clipping': False}))


if __name__ == '__main__':
    main()
