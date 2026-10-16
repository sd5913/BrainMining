import hashlib
import json
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved/AudioPilotTools'))
import soundfile as sf

SOURCE = ROOT / 'SourceAssets/MainGameAudio'
OUTPUT = SOURCE / 'Runtime'
RATE = 48000
records = []


def load(relative):
    samples, rate = sf.read(ROOT / relative, dtype='float32', always_2d=True)
    if rate != RATE:
        timeline = np.arange(round(len(samples) * RATE / rate)) * rate / RATE
        samples = np.stack([np.interp(timeline, np.arange(len(samples)), samples[:, channel]) for channel in range(samples.shape[1])], axis=1).astype('float32')
    return samples


def write(name, paths, loop=False, category='Effects', end=None, blend=False):
    samples = load(paths[0])
    if category == 'Effects':
        samples = samples.mean(axis=1, keepdims=True)
    if end:
        samples = samples[:round(end * RATE)]
    if len(paths) > 1:
        texture = load(paths[1]).mean(axis=1, keepdims=True)
        texture = np.tile(texture, (int(np.ceil(len(samples) / len(texture))), 1))[:len(samples)]
        samples = samples * .75 + texture * .18
    if blend:
        overlap = min(round((3 if category == 'Music' else .15) * RATE), len(samples) // 8)
        ramp = np.linspace(0, 1, overlap)[:, None]
        seam = samples[-overlap:] * (1 - ramp) + samples[:overlap] * ramp
        samples = np.concatenate([samples[overlap:-overlap], seam])
    if not loop:
        overlap = min(240, len(samples) // 8)
        samples[:overlap] *= np.linspace(0, 1, overlap)[:, None]
        samples[-overlap:] *= np.linspace(1, 0, overlap)[:, None]
    peak = np.max(np.abs(samples))
    if not np.isfinite(samples).all() or peak <= 1e-6:
        raise ValueError(name)
    samples *= min(1., (.4 if category != 'Effects' else .6) / peak)
    destination = OUTPUT / (name + '.wav')
    sf.write(destination, samples, RATE, subtype='PCM_16')
    samples, rate = sf.read(destination, always_2d=True)
    assert np.isfinite(samples).all() and np.max(np.abs(samples)) < .999
    records.append(dict(asset=name, file=str(destination.relative_to(SOURCE)).replace('\\', '/'),
                        loop=loop, category=category, channels=samples.shape[1], sample_rate=rate,
                        seconds=round(len(samples) / rate, 4), peak=float(np.max(np.abs(samples))),
                        sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                        inputs=[dict(file=path, sha256=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()) for path in paths],
                        processing=dict(trim_seconds=end, texture_mix=[.75,.18] if len(paths)>1 else None,
                                        loop_crossfade=blend, preserve_candidate_processing=True)))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    prefix = 'SourceAssets/MainGameAudio/Candidates/'
    mappings = {
        'S_PickaxePickup': ('C_PickaxePickup', .8),
        'S_JetpackEquip': ('C_JetpackEquip', .65),
        'S_DirtDig': ('C_DirtDig', .55),
        'S_Foot_dirt_00': ('C_Foot_mud02', None),
        'S_Foot_gravel_00': ('C_Foot_gravel', .5),
        'S_DrillStart': ('C_DrillStart', None),
        'S_DrillIdle_Loop': ('C_DrillMotor_Loop', None),
        'S_DrillStop': ('C_DrillStop', .7),
        'S_JetStart': ('C_JetStart', None),
        'S_Jet_Loop': ('C_JetThrust_00_Loop', None),
        'S_JetStop': ('C_JetVent_01', .65),
    }
    for name, (source, end) in mappings.items():
        write(name, [prefix + source + '.wav'], loop=name.endswith('_Loop'), end=end)
    write('S_BedrockContact', ['SourceAssets/AudioPilot/Wav/S_DigLight_00.wav'], end=.18)
    write('S_DrillContact_Loop', [prefix + 'C_DrillTexture_Loop.wav', 'SourceAssets/AudioPilot/Wav/S_DigBreak_00.wav'], loop=True, blend=True)
    write('S_DrillDirt_Loop', [prefix + 'C_DrillMotor_Loop.wav', prefix + 'C_DirtDig.wav'], loop=True, blend=True)
    write('S_BombExplosion', [prefix + 'C_BombCrunch_01.wav', 'SourceAssets/AudioPilot/Wav/S_DigBreak_01.wav'])
    for name, source in [('A_MineWind_Loop','C_Amb_Wind_Loop'),('A_Cavern_Loop','C_Amb_Cavern_Loop'),('A_Drips_Loop','C_Amb_Drips_Loop')]:
        write(name, [prefix + source + '.wav'], loop=True, category='Ambience')
    write('M_CaveTheme_Loop', ['SourceAssets/MainGameAudio/Originals/cave_music/cave_music.ogg'], loop=True, category='Music', blend=True)
    (SOURCE/'runtime_manifest.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
    selected = ['workshop','shovel','steps','kenney_scifi','steam','wind','dungeon','drips','cave_music']
    sources = json.loads((SOURCE/'sources.json').read_text(encoding='utf-8'))
    credits = ['MainGame additional audio — 2026-10-16',
               'Adaptations: excerpting, mono conversion for effects, 48kHz PCM, fades, loop crossfades, gain adjustment and texture mixing.',
               'These selected additional sources use CC0. Original page evidence is retained in SourceAssets/MainGameAudio/Licenses/Pages.', '']
    for source in sources:
        if source['id'] in selected:
            credits.extend([source['title']+' — '+source['author'],source['page_url'],source['license_url'],''])
    credits.extend(['Existing Kenney/rubberduck texture layers and Dreamy Flashback retain the credits in AudioPilotCredits/CREDITS.txt.',
                    'Deep Haze, EmptyCity, Michel Baradari explosions and neural force-field alternatives are not used by this MainGame selection.'])
    destination = ROOT/'Content/MainGameAudioCredits'
    destination.mkdir(parents=True,exist_ok=True)
    (destination/'CREDITS.txt').write_text('\n'.join(credits)+'\n',encoding='utf-8')
    print(json.dumps(dict(assets=len(records),seconds=sum(record['seconds'] for record in records),clipping=False)))


if __name__ == '__main__':
    main()
