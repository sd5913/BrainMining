import concurrent.futures
import hashlib
import json
import pathlib
import sys
import urllib.parse
import urllib.request

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved/AudioPilotTools'))
import soundfile as sf

SOURCE = ROOT / 'SourceAssets/RoomMusic'
RATE = 48000
TRACKS = [
    ('M_Room_Office_Loop', 'Lightless Dawn', 'USUAN1100655'),
    ('M_Room_CardExplore_Loop', 'Enchanted Journey', 'USUAN1100799'),
    ('M_Room_CardBattle_Loop', 'Mischief Maker', 'USUAN1700009'),
    ('M_Room_Escape_Loop', 'Run Amok', 'USUAN1400024'),
    ('M_Room_Museum_Loop', 'Meditation Impromptu 01', 'USUAN1100163'),
]


def download(url, destination):
    if destination.exists():
        return destination.read_bytes()
    request = urllib.request.Request(url, headers={'User-Agent': 'BrainMining audio asset import'})
    with urllib.request.urlopen(request, timeout=90) as response:
        data = response.read()
    if not data:
        raise RuntimeError('Empty download: ' + url)
    destination.write_bytes(data)
    return data


def prepare(track, catalog):
    asset, title, isrc = track
    metadata = next(entry for entry in catalog if entry['isrc'] == isrc)
    if metadata['title'] != title:
        raise RuntimeError('Track identity mismatch: ' + title)
    page_url = 'https://incompetech.com/music/royalty-free/index.html?isrc=' + isrc
    page = download(page_url, SOURCE / 'Licenses' / (isrc + '.html'))
    if b'By Attribution 4.0' not in page:
        raise RuntimeError('Expected attribution not found: ' + title)
    (SOURCE / 'Licenses' / (isrc + '.json')).write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    original = SOURCE / 'Originals' / metadata['filename']
    audio_url = 'https://incompetech.com/music/royalty-free/mp3-royaltyfree/' + urllib.parse.quote(metadata['filename'])
    data = download(audio_url, original)
    samples, original_rate = sf.read(original, dtype='float32', always_2d=True)
    original_seconds = len(samples) / original_rate
    if samples.shape[1] != 2 or original_seconds < 60:
        raise RuntimeError('Invalid stereo music: ' + title)
    if original_rate != RATE:
        timeline = np.arange(round(len(samples) * RATE / original_rate)) * original_rate / RATE
        samples = np.stack([np.interp(timeline, np.arange(len(samples)), samples[:, channel]) for channel in range(2)], axis=1).astype('float32')
    samples -= np.mean(samples, axis=0)
    audible = np.flatnonzero(np.max(np.abs(samples), axis=1) > .001)
    if not len(audible):
        raise RuntimeError('Silent track: ' + title)
    start = max(0, int(audible[0]) - 2400)
    end = min(len(samples), int(audible[-1]) + 2401)
    samples = samples[start:end]
    overlap = round(1.5 * RATE)
    ramp = np.linspace(0, np.pi / 2, overlap, dtype='float32')[:, None]
    seam = samples[-overlap:] * np.cos(ramp) + samples[:overlap] * np.sin(ramp)
    samples = np.concatenate([samples[overlap:-overlap], seam])
    samples[:240] *= np.linspace(0, 1, 240, dtype='float32')[:, None]
    samples[-240:] *= np.linspace(1, 0, 240, dtype='float32')[:, None]
    peak = float(np.max(np.abs(samples)))
    rms = float(np.sqrt(np.mean(samples.astype('float64') ** 2)))
    samples *= min(.13 / max(rms, 1e-8), .85 / max(peak, 1e-8))
    output = SOURCE / 'Wav' / (asset + '.wav')
    sf.write(output, samples, RATE, subtype='PCM_16')
    rendered, rate = sf.read(output, dtype='float32', always_2d=True)
    peak = float(np.max(np.abs(rendered)))
    if rate != RATE or not np.isfinite(rendered).all() or peak >= .86 or peak < .05:
        raise RuntimeError('Waveform validation failed: ' + title)
    if np.max(np.abs(rendered[[0, -1]])) > 1 / 32768:
        raise RuntimeError('Loop endpoint validation failed: ' + title)
    record = dict(asset=asset, title=title, author='Kevin MacLeod', isrc=isrc,
                  file='Wav/' + output.name, category='Music', loop=True,
                  channels=2, sample_rate=RATE, seconds=len(rendered) / RATE,
                  peak=peak, rms=float(np.sqrt(np.mean(rendered.astype('float64') ** 2))),
                  first_two_seconds_rms=float(np.sqrt(np.mean(rendered[:RATE * 2].astype('float64') ** 2))),
                  sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  source_url=page_url, download_url=audio_url, original='Originals/' + original.name,
                  original_sha256=hashlib.sha256(data).hexdigest(), original_seconds=original_seconds,
                  license='CC-BY-4.0', license_url='https://creativecommons.org/licenses/by/4.0/',
                  processing=dict(trim_start_seconds=start / RATE, trim_end_seconds=end / RATE,
                                  crossfade_seconds=1.5, seam_fade_seconds=.005, target_rms=.13, peak_limit=.85))
    print(asset + ' prepared', flush=True)
    return record


def main():
    for folder in ('Originals', 'Licenses', 'Wav'):
        (SOURCE / folder).mkdir(parents=True, exist_ok=True)
    catalog = json.loads(download('https://incompetech.com/music/royalty-free/pieces.json', SOURCE / 'Licenses/catalog.json'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        entries = list(pool.map(lambda track: prepare(track, catalog), TRACKS))
    (SOURCE / 'manifest.json').write_text(json.dumps(entries, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    credits = ['BrainMining room music', '']
    for entry in entries:
        credits.extend([entry['title'] + ' — Kevin MacLeod (incompetech.com)',
                        entry['source_url'],
                        'Licensed under Creative Commons: By Attribution 4.0 License',
                        entry['license_url'],
                        'Changes: silence trimmed, 48 kHz stereo PCM conversion, loop crossfade, endpoint fades and gain adjustment.', ''])
    text = '\n'.join(credits)
    (SOURCE / 'Licenses/CREDITS.txt').write_text(text, encoding='utf-8')
    packaged = ROOT / 'Content/RoomMusicCredits'
    packaged.mkdir(parents=True, exist_ok=True)
    (packaged / 'CREDITS.txt').write_text(text, encoding='utf-8')
    print('ROOM_MUSIC_PREPARE_OK: ' + str(len(entries)), flush=True)


if __name__ == '__main__':
    main()
