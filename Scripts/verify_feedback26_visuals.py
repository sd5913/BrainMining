"""Opt-in runtime player-feedback checks and visual captures; isolated profile only."""
import argparse,json,subprocess,time,traceback,math
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');a=ap.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True);profile=out/'profile';assert not profile.exists();profile.mkdir()
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
proc=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=870','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si);c=Client(profile);r={'checks':[],'passed':False}
def cmd(op,**kw):
 x=c.command(op,timeout=180,**kw);assert x['ok'],(op,kw);return x
def check(ok,label):
 r['checks'].append(dict(passed=bool(ok),name=label));assert ok,label
 print('PASS',label,flush=True)
def wait(pred,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.15)
 raise AssertionError('State timeout '+str(s))
def shot(name):cmd('shot',name=name);time.sleep(.45)
def plus(p,q):return [p[i]+q[i] for i in range(3)]
try:
 wait(lambda s:s['opening']);cmd('start');wait(lambda s:not s['opening'])
 for tool in [0,1,2]:cmd('gm_tool',tool=tool)
 spec=next(x for x in json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8')) if x['id']==16);front=spec['clue'];n=spec['normal'];cmd('gm_jump',area=0)
 cmd('pose',eye=[front[i]+n[i]*240 for i in range(3)],target=front);time.sleep(1);shot('aroma-surface')
 obj=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==16 and x['position'][2]>0)
 cmd('mine',actor=obj['host'],point=front,radius=100);time.sleep(1);shot('aroma-surface-cut');check(bool(cmd('actors',filter='CollectionPersistentAroma')['actors']),'Food plume remains after first surface cut')
 spec=next(x for x in json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8')) if x['id']==19);front=spec['clue'];n=spec['normal'];cmd('gm_jump',area=2)
 cmd('pose',eye=[front[i]+n[i]*170 for i in range(3)],target=front);time.sleep(1);shot('citrus-slices')
 cmd('gm_jump',area=2);obj=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==3 and x['position'][2]>0);check(not obj['released'],'Pigeon starts buried in its soil');r['pigeon']=obj
 cmd('pose',eye=plus(obj['position'],[350,-450,60]),target=obj['position']);time.sleep(.6);shot('pigeon-buried')
 cmd('mine',actor=obj['host'],point=obj['position'],radius=180);time.sleep(1);check(next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x['path']==obj['path'])['released'],'Pigeon releases from actual soil excavation');shot('pigeon-exposed')
 cmd('gm_jump',area=4)
 for i,actor in enumerate(cmd('actors',filter='BM_Polish32_BombSign')['actors']):
  if actor['position'][0]<30000:continue
  pos=actor['position'];f=actor['forward'];cmd('pose',eye=[pos[k]-f[k]*250 for k in range(3)],target=[pos[k]+f[k]*80 for k in range(3)]);time.sleep(.6);shot('bomb-'+str(i))
 # Look back toward the final mine corridor from the known toilet approach.
 obj=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==0 and x['position'][2]>0);r['toilet']=obj
 cmd('pose',eye=plus(obj['position'],[-850,0,80]),target=plus(obj['position'],[-1600,0,80]));time.sleep(.7);shot('heaven-route')
 r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');proc.wait(timeout=30)
 except:proc.terminate()
if not r['passed']:raise SystemExit(1)
