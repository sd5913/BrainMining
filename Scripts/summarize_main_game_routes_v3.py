"""Summarize completed V3 checks without rerunning or mutating the editor."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Saved/Validation/MainGameRoutesV3'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


reports = {name: read(OUT / file) for name, file in {
    'authoring': 'authoring.json',
    'finishing': 'finishing.json',
    'saved': 'saved_state.json',
    'routes': 'runtime.json',
    'rooms': 'LinkedRooms/runtime.json',
    'native': 'native.json',
}.items()}
for name in ['authoring', 'finishing', 'saved', 'routes', 'rooms']:
    assert reports[name]['passed'], (name, reports[name].get('error'))
native = reports['native']
assert native['total'] == native['passed'] == 6 and native['failed'] == 0
assert all(t['state'] == 'Success' and not t['errors'] for t in native['tests'])
assert 'Result: Succeeded' in (OUT / 'runtime_offset_build.log').read_text(encoding='utf-8', errors='replace')
stages = ['TunnelV1', 'TunnelCavernV2', 'TunnelStalagmiteV3', 'TunnelNeuralV4']
for stage in stages[:3]:
    source = ROOT / 'Art/MainGame/RoutesV3' / stage
    assert read(source / 'validation.json')['passed'], stage
before = read(OUT / 'before.json')
for source, digest in before['source_hashes'].items():
    assert hashlib.sha256((ROOT / 'Content' / (source[6:] + '.umap')).read_bytes()).hexdigest() == digest, source
map_path = ROOT / 'Content/MainGame/Maps/Lvl_MainGame.umap'
assert map_path.stat().st_size > 90_000_000
result = {
    'passed': True,
    'verified_utc': datetime.now(timezone.utc).isoformat(),
    'map': str(map_path),
    'map_sha256': hashlib.sha256(map_path.read_bytes()).hexdigest(),
    'map_bytes': map_path.stat().st_size,
    'backup': reports['authoring']['backup'],
    'build': 'Succeeded',
    'script_checks': {name: len(reports[name]['checks']) for name in ['saved', 'routes', 'rooms']},
    'capsule_samples': sum(r['capsule_samples'] for r in reports['routes']['regions']),
    'physical_landmarks': sum(r['landmarks'] for r in reports['routes']['regions']),
    'connections': reports['rooms']['connections'],
    'office': reports['rooms']['office_validation'],
    'mirror': reports['rooms']['mirror_validation'],
    'native_passed': native['passed'],
    'native_warnings': [w for t in native['tests'] for w in t['warnings']],
    'source_maps_unchanged': list(before['source_hashes']),
    'regions': [{k: r[k] for k in ['stage', 'style', 'entry', 'exit', 'triangles', 'plan']} for r in reports['saved']['regions']],
    'limits': 'PIE tests use isolated area access and some positioning. They do not represent a human continuous end-to-end playthrough.',
}
(OUT / 'acceptance.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({k: result[k] for k in ['passed', 'script_checks', 'capsule_samples', 'physical_landmarks', 'native_passed']}, ensure_ascii=False))
