"""Verify recovery in separate Shipping processes and disposable user directories."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time
from packaged_qa_client import Client

parser=argparse.ArgumentParser()
parser.add_argument('exe')
parser.add_argument('source_user')
parser.add_argument('output')
args=parser.parse_args()
root=Path(args.output).resolve();root.mkdir(parents=True,exist_ok=True)
source=Path(args.source_user).resolve()/'Saved'/'SaveGames'
slot='BrainMiningProgress_v1'
results=[]
for label,corrupt in [('main', ['']),('main-and-pending',['','_pending']),('all',['','_pending','_backup'])]:
    user=root/label
    saves=user/'Saved'/'SaveGames'
    saves.mkdir(parents=True,exist_ok=True)
    for suffix in ('','_pending','_backup'):
        shutil.copy2(source/(slot+suffix+'.sav'),saves/(slot+suffix+'.sav'))
    for suffix in corrupt:
        (saves/(slot+suffix+'.sav')).write_bytes(b'INTERRUPTED-QA-SAVE')
    proc=subprocess.Popen([str(Path(args.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(user)])
    c=Client(user)
    try:
        state=c.command('state',timeout=90)
        if label=='all':
            assert not state['has_save'], 'invalid slots must not advertise Continue'
        else:
            assert state['has_save'], 'valid recovery slot must advertise Continue'
            state=c.command('continue')
            assert not state['opening'], 'recovery must release the opening menu'
            assert state['completed']==15 and state['upgrade']==1 and state['collection']==1, state
            assert state['prototypes']==4 and state['bomb'] and state['drill'], state
            expected=json.loads((Path(args.source_user)/'Saved'/'PackagedQA'/'terrain-expected.json').read_text(encoding='utf-8'))
            rock=next(a for a in c.command('actors',filter='BM_StartRoom_ExitBlocker')['actors'] if 'cuts' in a)
            assert rock['cuts']==expected['cuts'] and rock['triangles']==expected['triangles'], rock
        results.append(dict(case=label,passed=True,state=state))
        print('PASS recovery '+label,flush=True)
    finally:
        c.command('quit')
        closing=time.monotonic();proc.wait(timeout=90)
        print('Closed in %.1f seconds' % (time.monotonic()-closing),flush=True)
(root/'results.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
