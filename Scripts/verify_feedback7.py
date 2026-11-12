"""Opt-in runtime player-feedback checks and visual captures; isolated profile only."""
import argparse,json,subprocess,time,traceback,math
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');a=ap.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True);profile=out/'profile';assert not profile.exists();profile.mkdir()
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
proc=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=870','-NoSteam','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si);c=Client(profile);r={'checks':[],'passed':False}
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
def item(Id):return next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==Id and x['position'][2]>0 and 'CollectionFlying' not in x['name'])
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def norm(v):return [x/(sum(y*y for y in v)**.5) for x in v]
try:
 wait(lambda s:s['opening']);cmd('start');wait(lambda s:not s['opening'])
 for tool in [0,1,2]:cmd('gm_tool',tool=tool)
 specs=json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8'))
 # Steam remains visible after removing the paw's supporting surface, from three viewing angles.
 cmd('gm_jump',area=0);spec=next(x for x in specs if x['id']==16);front=spec['clue'];n=spec['normal'];obj=item(16)
 for label,offset in [('front',[0,0,0]),('side',[0,170,40])]:
  eye=[front[i]+n[i]*240+offset[i] for i in range(3)];cmd('pose',eye=eye,target=plus(front,[0,0,50]));time.sleep(.8);shot('steam-'+label)
 cmd('mine',actor=obj['host'],point=front,radius=120);time.sleep(.5);shot('steam-after-cut');check(bool(cmd('actors',filter='CollectionPersistentAroma')['actors']),'Steam source survives the first soil cut')
 # Multi-layer shell bank, with each excavated support removed rather than floating.
 spec=next(x for x in specs if x['id']==17);front=spec['clue'];n=spec['normal'];obj=item(17)
 check(abs(sum((obj['position'][i]-front[i])**2 for i in range(3))**.5-400)<2,'Fishbowl is buried 400 cm behind the clue')
 counts=[]
 for depth in [0,80,160,240]:
  if depth:cmd('mine',actor=obj['host'],point=[front[i]-n[i]*(depth-60) for i in range(3)],radius=110)
  cmd('pose',eye=[front[i]+n[i]*(200-depth) for i in range(3)],target=[front[i]-n[i]*(depth+80) for i in range(3)]);time.sleep(.5);counts.append(len(cmd('actors',filter='BranchClue_17')['actors']));shot('shell-layer-'+str(depth))
 check(counts[0]==42 and counts[-1]>=7 and counts[-1]<counts[0],'Shells remain in deeper layers; removed supports do not leave floating shells');r['shell_counts']=counts
 # Reveal at 40%, keep the train fixed, and wait for three seconds of crowd before pickup.
 cmd('gm_jump',area=4);spec=next(x for x in specs if x['id']==21);front=spec['clue'];n=spec['normal'];obj=item(21)
 cmd('pose',eye=[front[i]+n[i]*250 for i in range(3)],target=obj['position']);time.sleep(.5)
 check(item(21)['crowd_parts']==0,'Subway crowd is hidden while buried')
 cmd('mine',actor=obj['host'],point=front,radius=65);time.sleep(.2);check(not item(21)['released'] and item(21)['crowd_parts']==0,'First soil chip does not reveal people before the train');shot('subway-first-chip')
 # Incremental real cuts reach the shared reveal/release threshold.
 partial=None
 for radius in range(35,181,10):
  cmd('mine',actor=obj['host'],point=[obj['position'][i]+n[i]*50 for i in range(3)],radius=radius);time.sleep(.16);partial=item(21)
  if partial['exposure']>=.4:break
 check(.4<=partial['exposure']<.7 and partial['released'] and not partial['pickup_ready'],'Doors begin at 40 percent exposure while pickup waits');r['subway_partial']=partial
 time.sleep(1.5);after=item(21)
 check(after['crowd_parts']==108 and abs(after['door'][1])>=33 and not after['pickup_ready'],'Train doors and crowd appear before the pickup delay expires');shot('subway-partially-exposed')
 check(math.dist(after['position'],obj['position'])<.1,'Train stays steady while the crowd presentation is protected')
 time.sleep(2.5);after=item(21)
 check(after['released'] and after['pickup_ready'] and after['exposure']<.7,'Three seconds of crowd unlocks pickup without another cut')
 check(math.dist(after['position'][:2],obj['position'][:2])<.1 and after['position'][2]<=obj['position'][2]+.1,'Train can settle downward but never pops outward');shot('subway-crowd')
 # The resident pigeon faces the player after a side/back excavation.
 cmd('gm_jump',area=2);obj=item(3);eye=plus(obj['position'],[300,350,80]);cmd('pose',eye=eye,target=obj['position']);cmd('mine',actor=obj['host'],point=obj['position'],radius=250);time.sleep(.3);bird=item(3)
 check(bird['released'] and dot([-x for x in bird['forward']],norm([eye[i]-bird['position'][i] for i in range(3)]))>.97,'Pigeon faces the side/back excavator');shot('pigeon-facing-player')
 # Catalogue: flap on both views, bigger chemical bubbles, open subway with continuous crowd.
 c.key('C')
 for Id in [3,2,21]:
  cmd('catalogue_owned_fixture',item=Id);cmd('catalogue_rotate',item=Id,yaw=0,pitch=0);time.sleep(.5);one=cmd('state')['catalogue_motion'];time.sleep(.31);two=cmd('state')['catalogue_motion'];shot('catalogue-'+str(Id))
  check(one[1:]!=two[1:],f'Catalogue item {Id} animates while paused')
  if Id==2:check(len(two)==25,'Chemistry has 24 enhanced bubbles')
  if Id==21:check(one[-8:]==two[-8:],'Catalogue train doors remain open without cycling')
 c.key('C')
 # Max-level pickaxe and lamp at the sky wall, diagonal deep excavation and outside the old box.
 for typ in ['2','4','6','9']:cmd('mineral_fixture',mineral=typ,amount=200)
 while cmd('state')['upgrade']<4:cmd('upgrade')
 cmd('gm_jump',area=4);center=[36238.500305,16376.952185,18112.792084]
 probes=[]
 for name,offset in [('inside',[900,900,180]),('diagonal-earth',[1750,1750,180]),('deep-earth',[2200,1800,180])]:
  eye=plus(center,offset);target=plus(eye,[240,240,0]);cmd('pose',eye=eye,target=target);time.sleep(.3)
  # Excavate the same player-sized pocket through whichever soil currently occupies this point.
  for _ in range(5):
   h=cmd('trace',**{'from':plus(eye,[-450,-450,0]),'to':target})
   if not h.get('diggable'):break
   cmd('mine',actor=h['hit_actor'],point=eye,radius=200)
  cmd('pose',eye=eye,target=target);time.sleep(1.2);s=cmd('state');shot('light-'+name);probes.append({'name':name,'weight':s.get('heaven_exposure_weight'),'lamps':s['lamps'],'camera':s['camera']})
  check(all(x['visible'] and x['intensity']>0 for x in s['lamps'] if x['name'] in ['MiningLight','MiningFillLight']),name+' headlamp remains enabled')
  check((s['heaven_exposure_weight']>.99) if name=='inside' else s['heaven_exposure_weight']==0,name+' uses the correct room exposure weight')
 r['lighting_probes']=probes;r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');proc.wait(timeout=30)
 except:proc.terminate()
if not r['passed']:raise SystemExit(1)
