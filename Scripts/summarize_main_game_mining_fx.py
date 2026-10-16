"""Summarize the completed mining-effect regression and saved-state checks."""
import json,hashlib
from pathlib import Path
from datetime import datetime,timezone
root=Path(__file__).resolve().parents[1];out=root/'Saved/Validation/MainGameMiningFX'
def read(name):return json.loads((out/name).read_text(encoding='utf-8'))
runtime=read('runtime.json');before=read('reproduced_before.json');prebuild=read('prebuild.json');native=read('native.json')
assert read('saved.json')['passed']
assert runtime['passed'] and runtime['completed'] and len(runtime['samples'])==16,runtime.get('error')
assert any(not c['tick'] for s in before['samples'] for c in s['clouds']),'Missing reproduction of the suspended impact effect'
assert 'Result: Succeeded' in (out/'native_build.log').read_text(encoding='utf-8',errors='replace')
assert native['total']==native['passed']==1 and native['failed']==0
assert all(not t['errors'] for t in native['tests'])
for p,digest in prebuild['protected_hashes'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
summary=dict(passed=True,verified_utc=datetime.now(timezone.utc).isoformat(),backup=prebuild['backup'],regions=4,impact_cases=len(runtime['samples']),checks=len(runtime['checks']),native_passed=native['passed'],protected_assets_unchanged=list(prebuild['protected_hashes']),limits='PIE fixtures use isolated area entry, positioning and real upgrade purchases. No continuous human playthrough is claimed.')
(out/'acceptance.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(summary,ensure_ascii=False))
