"""Sample two autosave intervals with a real edited-terrain checkpoint."""
import argparse,shutil,subprocess,time,json
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('output');p.add_argument('fixture');a=p.parse_args()
out=Path(a.output).resolve();profile=out/'profile';shutil.copytree(a.fixture,profile/'Saved/SaveGames')
proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
c=Client(profile)
try:
 c.command('state',timeout=180);r=c.command('continue',timeout=180);assert r['ok'],r
 time.sleep(5)
 state=c.command('state');expected={}
 rocks={r['path']:r for r in c.command('actors',filter='DiggableRock')['actors']}
 for path in state['computer_rocks'][2:6]:
  r=c.command('mine',actor=path,point=rocks[path]['center'],radius=180);assert r['ok'],r
  time.sleep(1.5)
 for r in c.command('actors',filter='DiggableRock')['actors']:
  if r['path'] in state['computer_rocks'][2:6]:expected[r['path'].split(':')[-1]]=(r['cuts'],r['triangles'])
 assert all(cuts>0 for cuts,triangles in expected.values())
 c.command('walking_fixture')
 results=[]
 for i in range(3):
  r=c.sample('periodic-'+str(i),45);results.append(r);print(json.dumps(r['metrics']),flush=True)
 final=c.command('state');assert final['autosaves']>=2,final;print('AUTOSAVES '+str(final.get('autosaves'))+' snapshot_ms='+str(final.get('snapshot_ms')),flush=True)
 (out/'results.json').write_text(json.dumps(dict(samples=results,final=final),ensure_ascii=False,indent=2),encoding='utf8')
finally:
 if proc.poll() is None:c.command('quit');proc.wait(timeout=60)

# Restart without a manual save: prove background snapshots retained the new cuts.
proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
try:
 c.command('state',timeout=180);assert c.command('continue',timeout=180)['ok'];time.sleep(5)
 actual={r['path'].split(':')[-1]:(r['cuts'],r['triangles']) for r in c.command('actors',filter='DiggableRock')['actors']}
 assert all(actual[k]==v for k,v in expected.items()),(expected,actual)
 print('PASS asynchronous terrain snapshots survive restart',flush=True)
 (out/'autosave-reload.json').write_text(json.dumps(dict(passed=True,meshes=expected),indent=2),encoding='utf8')
finally:
 if proc.poll() is None:c.command('quit');proc.wait(timeout=60)
