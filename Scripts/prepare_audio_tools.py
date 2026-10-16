import json
import numpy as np
import prepare_audio_pilot as audio


def periodic_noise(seconds, center, spread, seed):
    count = round(audio.RATE * seconds)
    random = np.random.default_rng(seed)
    frequencies = np.fft.rfftfreq(count, 1 / audio.RATE)
    spectrum = np.fft.rfft(random.normal(size=count))
    spectrum *= np.exp(-.5 * ((frequencies - center) / spread) ** 2)
    spectrum[0] = 0
    samples = np.fft.irfft(spectrum, n=count)
    return samples / max(np.max(np.abs(samples)), .00001)


def peak(samples, gain=.55):
    samples = np.asarray(samples).reshape(-1, 1)
    return samples * gain / max(np.max(np.abs(samples)), .00001)


def one_shot(samples, gain=.55):
    return audio.clean(np.asarray(samples).reshape(-1, 1), gain, trim=False)


def main():
    manifest_path = audio.OUTPUT / 'manifest.json'
    existing = json.loads(manifest_path.read_text(encoding='utf-8'))
    audio.MANIFEST.clear()
    provenance = [{'pack': 'project_procedural', 'file': 'Scripts/prepare_audio_tools.py',
                   'author': 'BrainMining project', 'license': 'Project-authored synthesis; no external recording',
                   'url': ''}]
    def save(name, samples, loop=False, sources=None):
        audio.save(name, samples, sources or provenance,
                   'Deterministic project synthesis, 48kHz mono PCM16; periodic loops or 5ms edge fades; audition mix', loop)
    timeline = np.arange(audio.RATE * 2) / audio.RATE
    drill = .6 * np.sin(2 * np.pi * 110 * timeline) + .2 * np.sin(2 * np.pi * 330 * timeline)
    drill += .4 * periodic_noise(2, 1500, 850, 41) * (.7 + .3 * np.cos(2 * np.pi * 33 * timeline))
    contact = .25 * drill + periodic_noise(2, 2200, 1600, 42) * (.65 + .35 * np.cos(2 * np.pi * 47 * timeline))
    jet = periodic_noise(2, 650, 600, 43) + .12 * np.sin(2 * np.pi * 80 * timeline)
    save('S_DrillIdle_Loop', peak(drill, .4), True)
    save('S_DrillContact_Loop', peak(contact, .5), True)
    save('S_Jet_Loop', peak(jet, .48), True)
    for tool, body in [('Drill', drill), ('Jet', jet)]:
        length = round(audio.RATE * .3)
        envelope = np.sin(np.linspace(0, np.pi, length)) ** .7
        save('S_' + tool + 'Start', one_shot(body[:length] * envelope, .45))
        save('S_' + tool + 'Stop', one_shot(body[:length] * np.linspace(1, 0, length) ** 2, .35))
    for name, frequency in [('S_DrillLow', 620), ('S_JetOverheat', 420)]:
        warning_time = np.arange(round(audio.RATE * .5)) / audio.RATE
        envelope = np.maximum(0, np.sin(2 * np.pi * 5 * warning_time)) ** 2
        save(name, one_shot(np.sin(2 * np.pi * frequency * warning_time) * envelope, .38))
    throw_file = 'Audio/cloth1.ogg'
    save('S_BombThrow', audio.clean(audio.read_audio('rpg', throw_file), .4), sources=[audio.source('rpg', throw_file)])
    stick_file = 'Audio/impactSoft_heavy_000.ogg'
    save('S_BombStick', audio.clean(audio.read_audio('impact', stick_file), .55), sources=[audio.source('impact', stick_file)])
    fuse = periodic_noise(2, 3200, 1200, 44) * (.45 + .3 * np.cos(2 * np.pi * 8 * timeline))
    save('S_BombFuse_Loop', peak(fuse, .32), True)
    blast_time = np.arange(audio.RATE * 3) / audio.RATE
    blast = periodic_noise(3, 200, 350, 45) * np.exp(-blast_time * 5)
    blast += .6 * np.sin(2 * np.pi * (65 * blast_time - 8 * blast_time ** 2)) * np.exp(-blast_time * 7)
    debris_file = 'bfh1_rock_falling_01.ogg'
    debris = audio.clean(audio.read_audio('breaking', debris_file), .4)
    offset = round(audio.RATE * .15)
    count = min(len(debris), len(blast) - offset)
    blast[offset:offset + count] += debris[:count, 0] * .5
    save('S_BombExplosion', one_shot(blast, .7), sources=provenance + [audio.source('breaking', debris_file)])
    collect_time = np.arange(round(audio.RATE * .42)) / audio.RATE
    collect = np.sin(2 * np.pi * (480 * collect_time + 700 * collect_time ** 2)) * np.sin(np.pi * collect_time / .42) ** 2
    save('S_Collect', one_shot(collect, .4))
    new_names = {entry['asset'] for entry in audio.MANIFEST}
    combined = [entry for entry in existing if entry['asset'] not in new_names] + audio.MANIFEST
    manifest_path.write_text(json.dumps(combined, ensure_ascii=False, indent=2), encoding='utf-8')
    note = '\nP1 tool audio: deterministic project-authored synthesis in Scripts/prepare_audio_tools.py; no external recordings for motor/thrust/fuse/warning/collect tones. Bomb throw/stick and explosion debris reuse the credited CC0 Kenney/rubberduck recordings. These are trial sound designs, pending subjective listening approval.\n'
    for target in [audio.OUTPUT / 'Licenses/CREDITS.txt', audio.ROOT / 'Content/AudioPilotCredits/CREDITS.txt']:
        original = target.read_text(encoding='utf-8').split('\nP1 tool audio:')[0]
        target.write_text(original + note, encoding='utf-8')
    print(json.dumps({'new_assets': len(audio.MANIFEST), 'total_assets': len(combined)}))


if __name__ == '__main__':
    main()
