import hashlib
import io
import json
import pathlib
import sys
import zipfile

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved/AudioPilotTools'))
import soundfile as sf

OUTPUT = ROOT / 'SourceAssets/RoomAudio'
RATE = 48000
SOURCES = {entry['id']: entry for entry in json.loads((OUTPUT / 'sources.json').read_text(encoding='utf-8'))}
RECORDS = []


def layer(pack, member, gain=1., offset=0., speed=1.):
    with zipfile.ZipFile(OUTPUT / SOURCES[pack]['archive']) as archive:
        raw = archive.read(member)
    destination = OUTPUT / 'Originals' / pack / pathlib.PurePosixPath(member).name
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(raw)
    samples, rate = sf.read(io.BytesIO(raw), dtype='float32', always_2d=True)
    samples = samples.mean(axis=1)
    coordinates = np.arange(max(2, round(len(samples) * RATE / rate / speed))) * rate * speed / RATE
    samples = np.interp(coordinates, np.arange(len(samples)), samples)
    samples -= samples.mean()
    return samples * gain, round(offset * RATE), dict(pack=pack, member=member, sha256=hashlib.sha256(raw).hexdigest(), gain=gain, offset=offset, speed=speed)


def cue(name, *layers, duration=None, loop=False, echo=False, gain=.55):
    length = round(duration * RATE) if duration else max(len(samples) + offset for samples, offset, _ in layers)
    mixed = np.zeros(max(length, 4800), dtype=np.float64)
    for samples, offset, _ in layers:
        count = min(len(samples), len(mixed) - offset)
        if count > 0:
            mixed[offset:offset + count] += samples[:count]
    if echo:
        dry = mixed.copy()
        mixed = np.pad(mixed, (0, round(.95 * RATE)))
        for delay, level in [(.13, .34), (.27, .21), (.43, .13), (.64, .07)]:
            offset = round(delay * RATE)
            mixed[offset:offset + len(dry)] += dry * level
    mixed -= mixed.mean()
    if loop:
        overlap = min(round(.12 * RATE), len(mixed) // 8)
        blend = np.linspace(0, 1, overlap)
        seam = mixed[-overlap:] * (1 - blend) + mixed[:overlap] * blend
        mixed = np.concatenate((mixed[overlap:-overlap], seam))
    else:
        fade = min(round(.008 * RATE), len(mixed) // 8)
        mixed[:fade] *= np.linspace(0, 1, fade)
        mixed[-fade:] *= np.linspace(1, 0, fade)
    peak = np.max(np.abs(mixed))
    assert np.isfinite(mixed).all() and peak > 1e-7, name
    mixed *= gain / peak
    destination = OUTPUT / 'Wav' / (name + '.wav')
    destination.parent.mkdir(parents=True, exist_ok=True)
    sf.write(destination, mixed, RATE, subtype='PCM_16')
    RECORDS.append(dict(asset=name, file='Wav/' + name + '.wav', sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                        loop=loop, category='Ambience' if name.startswith('A_') else 'Effects', channels=1,
                        seconds=len(mixed) / RATE, peak=float(np.max(np.abs(mixed))),
                        rms=float(np.sqrt(np.mean(mixed * mixed))), sources=[record for _, _, record in layers],
                        processing=dict(mono=True, sample_rate=RATE, echo=echo, crossfade=loop, peak_target=gain)))


def main():
    def kenney(pack, name, **kwargs):
        return layer(pack, 'Audio/' + name + '.ogg', **kwargs)
    def simple(name, pack, member, **kwargs):
        cue(name, kenney(pack, member), **kwargs)
    for name, member in [('S_Confirm','confirmation_001'), ('S_Reject','error_004'), ('S_Open','open_002'), ('S_Close','close_002'),
                         ('S_Reply','click_001'), ('S_BombOffer','confirmation_003'), ('S_Warning','error_006'),
                         ('S_Checkpoint','confirmation_002'), ('S_ElectricalWarning','glitch_002'), ('S_Retry','back_002'),
                         ('S_Thought','maximize_003'), ('S_Swap','switch_004'), ('S_Transform','maximize_001'),
                         ('S_Shrink','minimize_003'), ('S_Grow','maximize_004'), ('S_Reset','back_004'),
                         ('S_CardVictory','confirmation_004'), ('S_CardReward','confirmation_002'), ('S_EchoExit','confirmation_001')]:
        simple(name, 'interface', member)
    for index in range(3):
        simple('S_OfficeNotify_%02d' % index, 'interface', 'select_%03d' % (index + 1))
        simple('S_Paper_%02d' % index, 'casino', 'card-slide-%d' % (index + 1))
        simple('S_CardSelect_%02d' % index, 'casino', 'card-place-%d' % (index + 1))
        simple('S_Glass_%02d' % index, 'impact', 'impactGlass_light_%03d' % index)
        cue('S_EchoKnock_%02d' % index, kenney('impact','impactMining_%03d' % index), echo=True)
    simple('S_CardDraw','casino','card-fan-1')
    simple('S_CardPlace','casino','card-shuffle')
    simple('S_Seat','impact','impactWood_medium_000')
    simple('S_CardLaser','scifi','laserLarge_000')
    simple('S_CardPoison','scifi','slime_000')
    simple('S_CardImpact','impact','impactPunch_heavy_000')
    simple('S_Seal','scifi','doorClose_001')
    simple('S_RewardDoor','scifi','doorOpen_001')
    simple('S_Phase','scifi','forceField_000')
    simple('S_Float','scifi','forceField_002')
    simple('S_Pottery','impact','impactGlass_medium_000')
    cue('S_Printer', layer('loops','machine_03.ogg', gain=.55), kenney('casino','card-shove-1',offset=.15), duration=.85)
    cue('S_OfficePanic', kenney('interface','glitch_003'), layer('loops','alarm_01.ogg',gain=.25), duration=1.4)
    cue('S_BombPickup',kenney('casino','cards-pack-take-out-1'),kenney('interface','confirmation_001',offset=.1,gain=.55))
    cue('S_DoorBreak',kenney('impact','impactWood_heavy_000'),kenney('scifi','impactMetal_000',gain=.6),duration=1.)
    cue('S_Collapse',kenney('scifi','lowFrequency_explosion_000',speed=.65),kenney('scifi','explosionCrunch_001',offset=.3,speed=.7),echo=True)
    cue('S_CardArrival',kenney('interface','minimize_009'),kenney('impact','impactSoft_heavy_000',offset=.7),duration=1.4)
    cue('S_CardFire',kenney('scifi','thrusterFire_000',gain=.45),kenney('scifi','explosionCrunch_002',offset=.12,gain=.8),duration=1.25)
    cue('S_CardFinale',kenney('scifi','forceField_001',gain=.35),kenney('scifi','laserLarge_004',offset=.35,gain=.8),kenney('scifi','lowFrequency_explosion_001',offset=.95),duration=2.)
    cue('S_LowHealth',kenney('interface','error_007',gain=.6),kenney('impact','impactSoft_heavy_001',gain=.8),duration=.7)
    cue('S_EchoCrumble',kenney('scifi','explosionCrunch_003',gain=.45),kenney('impact','impactMining_002',speed=.65),echo=True)
    cue('S_EchoLanding',kenney('impact','impactSoft_heavy_000',speed=.8),kenney('impact','impactMining_000',gain=.25),echo=True)
    cue('S_FloodStart',layer('water','splash_06.ogg'),layer('water','loop_water_02.ogg',offset=.15,gain=.7),duration=1.5)
    cue('S_Drown',layer('water','bubble_02.ogg',speed=.75),kenney('interface','error_004',offset=.25,gain=.25),duration=1.4)
    cue('S_ShortCircuit',kenney('interface','glitch_004'),kenney('scifi','computerNoise_002',gain=.6),duration=.8)
    cue('S_Escape',kenney('interface','confirmation_004'),kenney('interface','confirmation_001',offset=.25,gain=.5),duration=1.4)
    cue('S_Pressure',kenney('scifi','impactMetal_002',speed=.8),kenney('interface','switch_001',offset=.15),duration=.8)
    cue('S_Power',kenney('scifi','computerNoise_000',gain=.3),kenney('interface','confirmation_002',offset=.08),duration=.8)
    cue('S_PowerOff',kenney('interface','glitch_001',gain=.3),kenney('interface','close_001',offset=.1),duration=.45)
    cue('S_Complete',kenney('interface','confirmation_004'),kenney('impact','impactBell_heavy_000',gain=.3,offset=.2,speed=.8),echo=True)
    cue('S_OfficeBurst_Loop',layer('loops','machine_06.ogg',gain=.4),layer('loops','noise_01.ogg',gain=.2),loop=True,gain=.4)
    cue('S_Water_Loop',layer('loops','water_flowing.ogg',gain=.6),layer('water','loop_water_02.ogg',gain=.8),loop=True,gain=.45)
    cue('S_Mechanism_Loop',layer('loops','rolling.ogg'),loop=True,gain=.35)
    cue('A_Office_Loop',layer('loops','ambient_02.ogg'),loop=True,gain=.25)
    cue('A_Giant_Loop',layer('loops','ambient_01.ogg'),loop=True,gain=.2)
    cue('A_Museum_Loop',layer('loops','ambient_03.ogg'),loop=True,gain=.2)
    cue('A_TV_Loop',layer('loops','machine_02.ogg',gain=.4),layer('loops','noise_02.ogg',gain=.2),loop=True,gain=.25)
    (OUTPUT / 'manifest.json').write_text(json.dumps(RECORDS,ensure_ascii=False,indent=2) + '\n',encoding='utf-8')
    credits = ['MainGame room audio — 2026-10-16', 'Selected packs: CC0-1.0. Pack-level evidence and original downloads are retained in SourceAssets/RoomAudio.',
               'Adaptations: selection, mono conversion, resampling, gain changes, layering, pitch/time changes, fades and loop crossfades.',
               'Printer and machinery sounds are designed composites, not recordings of the depicted devices.', '']
    for source in SOURCES.values():
        credits.extend([source['title'] + ' — ' + source['author'],source['page_url'],source['license_url'],''])
    destination = ROOT / 'Content/RoomAudioCredits'
    destination.mkdir(parents=True,exist_ok=True)
    (destination / 'CREDITS.txt').write_text('\n'.join(credits) + '\n',encoding='utf-8')
    print('Prepared',len(RECORDS),'room audio assets')


if __name__ == '__main__':
    main()
