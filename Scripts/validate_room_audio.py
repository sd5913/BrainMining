import array
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import wave
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/RoomAudio'


def main():
    entries = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
    packs = json.loads((SOURCE / 'sources.json').read_text(encoding='utf-8'))
    assert len(entries) == 71
    assert len({entry['asset'] for entry in entries}) == len(entries)
    source_hashes = {}
    for pack in packs:
        archive_path = SOURCE / pack['archive']
        assert hashlib.sha256(archive_path.read_bytes()).hexdigest() == pack['sha256']
        assert pack['license'] == 'CC0-1.0'
        evidence = (SOURCE / pack['evidence']).read_text(encoding='utf-8')
        assert 'cc0' in evidence.lower() or 'creative commons zero' in evidence.lower()
        with zipfile.ZipFile(archive_path) as archive:
            for entry in entries:
                for layer in entry['sources']:
                    key = (layer['pack'], layer['member'])
                    if layer['pack'] == pack['id'] and key not in source_hashes:
                        source_hashes[key] = hashlib.sha256(archive.read(layer['member'])).hexdigest()
    total_bytes = 0
    metrics = []
    for entry in entries:
        filename = SOURCE / entry['file']
        raw = filename.read_bytes()
        total_bytes += len(raw)
        assert hashlib.sha256(raw).hexdigest() == entry['sha256'], filename
        for layer in entry['sources']:
            assert source_hashes[(layer['pack'], layer['member'])] == layer['sha256']
        with wave.open(str(filename), 'rb') as wav:
            assert wav.getnchannels() == 1 and wav.getsampwidth() == 2 and wav.getframerate() == 48000
            samples = array.array('h', wav.readframes(wav.getnframes()))
            if sys.byteorder != 'little':
                samples.byteswap()
            peak = max(abs(value) for value in samples) / 32768
            rms = math.sqrt(sum(value * value for value in samples) / len(samples)) / 32768
            assert 0 < peak < .99 and rms > .0001, entry['asset']
            assert abs(len(samples) / 48000 - entry['seconds']) < .0001
            assert entry['loop'] == entry['asset'].endswith('_Loop')
            boundary = abs(samples[-1] - samples[0]) / 32768
            if entry['loop']:
                assert boundary < .08, (entry['asset'], boundary)
            metrics.append(dict(asset=entry['asset'], peak=peak, rms=rms, boundary_jump=boundary))
    files = [ROOT / 'Source/BrainMining/MiningRoomAudioSubsystem.cpp']
    for folder in ('Source/BrainMining', 'Plugins/CardRoom/Source', 'Plugins/EchoPassage/Source'):
        for filename in (ROOT / folder).rglob('*.cpp'):
            if 'Tests' not in filename.parts and filename not in files:
                text = filename.read_text(encoding='utf-8-sig')
                if 'MiningRoomAudioSubsystem::' in text or 'AudioOverride' in text:
                    files.append(filename)
    known = {entry['asset'] for entry in entries}
    aliases = {'S_Arrival': 'S_CardArrival', 'S_Draw': 'S_CardDraw', 'S_Laser': 'S_CardLaser',
               'S_Poison': 'S_CardPoison', 'S_Finale': 'S_CardFinale', 'S_Impact': 'S_CardImpact',
               'S_Victory': 'S_CardVictory', 'S_Reward': 'S_CardReward', 'S_Crumble': 'S_EchoCrumble',
               'S_Landing': 'S_EchoLanding', 'S_Knock': 'S_EchoKnock', 'S_Exit': 'S_EchoExit'}
    groups = ('S_OfficeNotify', 'S_Paper', 'S_CardSelect', 'S_Glass', 'S_EchoKnock')
    references = {}
    for filename in files:
        for line_number, line in enumerate(filename.read_text(encoding='utf-8-sig').splitlines(), 1):
            if filename.name == 'MiningAudioComponent.cpp' and 'MiningRoomAudioSubsystem::' not in line:
                continue
            for name in re.findall(r'TEXT\("([SA]_[A-Za-z0-9_]+)"\)', line):
                if name == 'A_':
                    continue
                name = aliases.get(name, name)
                targets = [name + '_%02d' % index for index in range(3)] if name in groups else [name]
                for target in targets:
                    assert target in known, (filename.name, line_number, target)
                    references.setdefault(target, []).append(f'{filename.relative_to(ROOT).as_posix()}:{line_number}')
    imported = json.loads((ROOT / 'Saved/RoomAudioImport.json').read_text(encoding='utf-8'))
    assert {item['asset'].split('.')[-1] for item in imported} == known
    for entry in entries:
        assert (ROOT / 'Content/Audio/Rooms' / (entry['asset'] + '.uasset')).stat().st_size > 1024
    report = dict(assets=len(entries), packs=len(packs), wav_bytes=total_bytes,
                  loops=sum(entry['loop'] for entry in entries),
                  referenced_assets=len(references), unreferenced=sorted(known - references.keys()),
                  metrics=metrics, references=references,
                  limitations='Static resource and reference checks only; native build and gameplay tests are separate.')
    (ROOT / 'Saved/RoomAudioValidation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in report.items() if key not in ('metrics', 'references')}, ensure_ascii=False))


if __name__ == '__main__':
    main()
