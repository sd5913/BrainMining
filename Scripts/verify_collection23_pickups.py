"""Exercise real excavation/pickup/save paths in an isolated Windows build."""
import argparse,json,subprocess,time,math,traceback
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('output');a=p.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);profile=out/'profile';assert not profile.exists()
c=Client(profile);report={'passed':False,'checks':[],'picked':[]};proc=None
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
def launch():
 global proc
 proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=startup)
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,kw);return r
def wait(pred,seconds=180):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.25)
 raise AssertionError(s)
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def shot(name):cmd('shot',name=name);time.sleep(.5)
def item(Id):return next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==Id and x['position'][2]>0)
try:
 launch();wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening']);cmd('gm_jump',area=0);time.sleep(1)
 cmd('pose',eye=[-30685,0,12125],target=[-30150,0,12150]);time.sleep(.7);shot('entrance-before-mining')
 authored=json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8'))
 bird=next(x for x in json.loads((root/'Saved/Validation/EmptyDeadEnds/saved-map.json').read_text(encoding='utf8'))['collectibles'] if x['id']==3)
 n=[bird['clue'][j]-bird['position'][j] for j in range(3)];d=math.sqrt(sum(v*v for v in n));bird.update(region=2,normal=[v/d for v in n]);authored.append(bird)
 for spec in authored:
  Id=spec['id'];cmd('gm_jump',area=(spec['region']-1)*2);time.sleep(.4)
  cmd('pose',eye=[spec['approach'][0],spec['approach'][1],spec['approach'][2]+55],target=spec['clue']);time.sleep(.5)
  before=item(Id);check(not before['released'],f'{Id} starts buried');host=before['host'];n=spec['normal'];pos=spec['position'];front=spec['clue']
  if Id==3:shot('pigeon-soil-before')
  for depth in [0,90,180]:
   at=[front[j]-n[j]*depth for j in range(3)];cmd('mine',actor=host,point=at,radius=195);time.sleep(.25)
  for attempt in range(12):
   obj=item(Id)
   if obj['released']:break
   time.sleep(.25)
  check(obj['released'],f'{Id} releases after real terrain cuts')
  # Track the physical drop before aiming, rather than moving the collectible.
  time.sleep(.6);obj=item(Id);q=obj['position'];eye=[q[j]+n[j]*150+(25 if j==2 else 0) for j in range(3)];cmd('pose',eye=eye,target=q);time.sleep(.4)
  shot('excavated-'+str(Id));cmd('collect');s=wait(lambda s:s['pickup_id']==-1 and s['collection_counts'][Id]>0)
  report['picked'].append(Id);check(True,f'{Id} collected with normal aim/E path and completed UI flight')
  if Id==3:check(True,'Pigeon wall is diggable without a bomb')
 check(cmd('state')['collection']==8,'Seven new items and pigeon are in inventory')
 cmd('save');wait(lambda s:not s['save_in_flight']);cmd('quit');proc.wait(timeout=60)
 launch();wait(lambda s:s['opening'] and s['menu_buttons']);cmd('continue');s=wait(lambda s:not s['opening'])
 check(len(s['collection_counts'])==23 and all(s['collection_counts'][Id]==1 for Id in report['picked']),'Restart/continue preserves all eight collected IDs in 23-slot inventory')
 for Id in report['picked']:check(item(Id)['collected'],f'{Id} stays collected after reload')
 c.key('C');time.sleep(.6);shot('saved-8-of23');cmd('catalogue_rotate',item=19,yaw=0,pitch=0);time.sleep(1);shot('cocktail-after-load');c.key('C')
 report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 if proc and proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=60)
  except Exception:pass
