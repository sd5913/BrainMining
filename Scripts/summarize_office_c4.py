"""Summarize completed office C4 asset, build and gameplay checks."""
import json,hashlib
from pathlib import Path
from datetime import datetime,timezone
root=Path(__file__).resolve().parents[1];out=root/'Saved/Validation/OfficeC4'
def read(name):return json.loads((out/name).read_text(encoding='utf-8'))
assets=read('assets.json');runtime=read('runtime.json');native=read('native.json');preparation=read('preparation.json');saved=read('saved.json')
assert saved['passed']
assert assets['passed'] and runtime['passed'],runtime.get('error')
assert native['total']==native['passed']==1 and native['failed']==0
assert native['tests'][0]['name']=='BrainMining.Office.ExitFlow' and not native['tests'][0]['errors']
assert 'Result: Succeeded' in (out/'native_build.log').read_text(encoding='utf-8',errors='replace')
for p,digest in preparation['protected_hashes'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
emitted=int(runtime['final_status'].split('bombs_emitted=')[1].split()[0]);old_estimate=6.5*max(2,runtime['evacuation_seconds'])*12;assert 0<emitted<old_estimate+24,emitted
result=dict(passed=True,verified_utc=datetime.now(timezone.utc).isoformat(),backup=preparation['backup'],assets=assets,script_checks=len(runtime['checks']),native_passed=native['passed'],finale_emitted=emitted,visible_at_capture=runtime['visible_c4'],protected_assets_unchanged=list(preparation['protected_hashes']),notes='Triangle budget is lower; frame rate improvement is not inferred from geometry counts. PIE fixtures use accelerated waiting and some positioning.')
(out/'acceptance.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
