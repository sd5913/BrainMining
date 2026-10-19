import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import zipfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved/AudioPilotTools'))
import soundfile as sf

OUTPUT = ROOT / 'SourceAssets/CollectionAudio'
ROOM = ROOT / 'SourceAssets/RoomAudio'
RATE = 48000
SOURCES = {source['id']: source for source in json.loads((ROOM / 'sources.json').read_text(encoding='utf-8'))}
RECORDS = []
USED_PACKS = set()


def layer(pack, filename, gain=1.0, offset=0.0, speed=1.0, start=0.0, duration=None, reverse=False):
    USED_PACKS.add(pack)
    if pack == 'rpg':
        raw = (ROOT / 'SourceAssets/AudioPilot/Originals/rpg' / filename).read_bytes()
        member = filename
    else:
        with zipfile.ZipFile(ROOM / SOURCES[pack]['archive']) as archive:
            matches = [member for member in archive.namelist() if Path(member).name == filename]
            if len(matches) != 1:
                raise ValueError(f'Ambiguous source: {pack}/{filename}')
            member = matches[0]
            raw = archive.read(member)
    original = OUTPUT / 'Originals' / pack / filename
    original.parent.mkdir(parents=True, exist_ok=True)
    original.write_bytes(raw)
    samples, rate = sf.read(io.BytesIO(raw), always_2d=True)
    samples = samples.mean(axis=1)
    first = round(start * rate)
    last = len(samples) if duration is None else min(len(samples), first + round(duration * rate))
    samples = samples[first:last]
    if len(samples) < 2:
        raise ValueError(f'Empty source: {filename}')
    if reverse:
        samples = samples[::-1]
    coordinates = np.arange(max(2, round(len(samples) * RATE / rate / speed))) * rate * speed / RATE
    samples = np.interp(coordinates, np.arange(len(samples)), samples)
    fade = min(round(.012 * RATE), len(samples) // 4)
    samples[:fade] *= np.linspace(0, 1, fade)
    samples[-fade:] *= np.linspace(1, 0, fade)
    provenance = dict(pack=pack, member=member, file=str(original.relative_to(ROOT)).replace('\\', '/'),
                      sha256=hashlib.sha256(raw).hexdigest(), gain=gain, offset=offset, speed=speed,
                      start=start, duration=duration, reverse=reverse)
    return samples * gain, round(offset * RATE), provenance


def cue(item_id, name, label, *layers):
    length = max(len(samples) + offset for samples, offset, _ in layers)
    mixed = np.zeros(length)
    for samples, offset, _ in layers:
        mixed[offset:offset + len(samples)] += samples
    taps = np.arange(41) - 20
    kernel = 2 * 4500 / RATE * np.sinc(2 * 4500 / RATE * taps) * np.hamming(41)
    mixed = np.convolve(mixed, kernel / kernel.sum(), mode='same')
    mixed -= mixed.mean()
    fade_in, fade_out = min(480, len(mixed) // 4), min(3840, len(mixed) // 4)
    mixed[:fade_in] *= np.linspace(0, 1, fade_in)
    mixed[-fade_out:] *= np.linspace(1, 0, fade_out)
    peak, rms = float(np.max(np.abs(mixed))), float(np.sqrt(np.mean(mixed * mixed)))
    if not np.isfinite(mixed).all() or rms < 1e-7:
        raise ValueError('Invalid cue: ' + name)
    mixed *= min(.11 / rms, .48 / peak)
    filename = OUTPUT / 'Wav' / (name + '.wav')
    filename.parent.mkdir(parents=True, exist_ok=True)
    sf.write(filename, mixed, RATE, subtype='PCM_16')
    RECORDS.append(dict(item_id=item_id, label=label, asset=name, file='Wav/' + filename.name,
                        sha256=hashlib.sha256(filename.read_bytes()).hexdigest(), category='Effects',
                        loop=False, channels=1, sample_rate=RATE, seconds=len(mixed) / RATE,
                        peak=float(np.max(np.abs(mixed))), rms=float(np.sqrt(np.mean(mixed * mixed))),
                        sources=[record for _, _, record in layers],
                        processing=dict(lowpass_hz=4500, peak_limit=.48, rms_target=.11, fade_out_seconds=.08)))


def main():
    cue(0, 'S_Collect_Toilet', '来自天堂的马桶',
        layer('water', 'splash_06.ogg', .7, duration=.65, speed=.85),
        layer('impact', 'impactBell_heavy_000.ogg', .16, offset=.16, speed=1.15, duration=.65))
    cue(1, 'S_Collect_AntComputer', '电脑里的蚂蚁窝',
        layer('scifi', 'computerNoise_000.ogg', .45, duration=.6, speed=.85),
        layer('interface', 'click_001.ogg', .25, offset=.10),
        layer('interface', 'click_001.ogg', .18, offset=.28, speed=1.13))
    cue(2, 'S_Collect_Reagent', '化学试剂',
        layer('impact', 'impactGlass_light_000.ogg', .24, duration=.3, speed=.85),
        layer('water', 'bubble_02.ogg', .65, offset=.08, duration=.65))
    cue(3, 'S_Collect_Pigeon', '肉鸽',
        layer('rpg', 'cloth1.ogg', .8, duration=.32, speed=.85),
        layer('rpg', 'cloth1.ogg', .55, offset=.22, duration=.3, speed=1.1),
        layer('rpg', 'cloth1.ogg', .28, offset=.41, duration=.24, speed=1.2))
    cue(4, 'S_Collect_Calculator', '奇怪的计算器',
        layer('interface', 'click_001.ogg', .3),
        layer('interface', 'click_001.ogg', .28, offset=.11, speed=.95),
        layer('interface', 'confirmation_001.ogg', .3, offset=.23, speed=.9, duration=.4))
    cue(5, 'S_Collect_Cassette', '损坏的磁带',
        layer('interface', 'switch_004.ogg', .4, speed=.8, duration=.25),
        layer('interface', 'glitch_001.ogg', .13, offset=.12, speed=.7, duration=.2),
        layer('interface', 'close_001.ogg', .3, offset=.36, speed=.8, duration=.25))
    cue(6, 'S_Collect_Note', '音符',
        layer('interface', 'pluck_001.ogg', .65, speed=.75, duration=.4),
        layer('interface', 'pluck_001.ogg', .4, offset=.18, speed=.9375, duration=.4),
        layer('interface', 'pluck_001.ogg', .25, offset=.36, speed=1.125, duration=.4))
    cue(7, 'S_Collect_Photo', '老旧的相片',
        layer('casino', 'card-slide-1.ogg', .65, speed=.8, duration=.55),
        layer('casino', 'card-place-1.ogg', .35, offset=.27, speed=.85, duration=.3))
    cue(8, 'S_Collect_Cola', '喷发中的可乐',
        layer('interface', 'switch_001.ogg', .3, duration=.12),
        layer('water', 'bubble_02.ogg', .65, offset=.06, speed=1.1, duration=.65),
        layer('water', 'splash_06.ogg', .2, offset=.16, speed=1.3, duration=.65))
    cue(9, 'S_Collect_Flower', '「GPT」之花',
        layer('interface', 'pluck_001.ogg', .3, speed=.65, duration=.3, reverse=True),
        layer('interface', 'pluck_001.ogg', .5, offset=.32, speed=.85, duration=.55))
    cue(10, 'S_Collect_Fireflies', '同步闪光的萤火虫',
        layer('interface', 'pluck_001.ogg', .48, speed=.9, duration=.18),
        layer('interface', 'pluck_001.ogg', .4, offset=.22, speed=.9, duration=.18),
        layer('interface', 'pluck_001.ogg', .32, offset=.44, speed=.9, duration=.25))
    cue(11, 'S_Collect_Ribbon', '散落的彩带',
        layer('casino', 'card-fan-1.ogg', .6, duration=.5, speed=.85),
        layer('interface', 'confirmation_004.ogg', .22, offset=.15, duration=.55, speed=.9))
    (OUTPUT / 'manifest.json').write_text(json.dumps(RECORDS, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    credits = ['BrainMining collectible pickup audio', 'All source recordings: CC0 1.0.',
               'https://creativecommons.org/publicdomain/zero/1.0/',
               'Edits: excerpts, layering, playback-rate changes, reversal where recorded, mono conversion,',
               '48 kHz PCM16 resampling, 4.5 kHz low-pass, fades and conservative loudness matching.', '']
    evidence = OUTPUT / 'Licenses'
    evidence.mkdir(parents=True, exist_ok=True)
    for pack in sorted(USED_PACKS):
        if pack == 'rpg':
            credits.append('RPG Audio — Kenney — https://kenney.nl/assets/rpg-audio')
            for filename in (ROOT / 'SourceAssets/AudioPilot/Licenses').glob('*'):
                if 'rpg' in filename.name.lower():
                    shutil.copyfile(filename, evidence / filename.name)
        else:
            source = SOURCES[pack]
            credits.append(f"{source['title']} — {source['author']} — {source['page_url']}")
            for filename in (ROOM / 'Licenses').glob(pack + '*'):
                shutil.copyfile(filename, evidence / filename.name)
    text = '\n'.join(credits) + '\n'
    (evidence / 'CREDITS.txt').write_text(text, encoding='utf-8')
    staged = ROOT / 'Content/CollectionAudioCredits'
    staged.mkdir(parents=True, exist_ok=True)
    (staged / 'CREDITS.txt').write_text(text, encoding='utf-8')
    print(json.dumps([dict(item_id=entry['item_id'], asset=entry['asset'], seconds=round(entry['seconds'], 3))
                      for entry in RECORDS], indent=2))


if __name__ == '__main__':
    main()
