"""Rendered checks on the actual MainGame pigeon wall, with an isolated profile."""
import argparse, json, math, time, traceback
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--baseline',action='store_true');ap.add_argument('--attach',action='store_true',required=True,help='Attach to the isolated game launched by Run_PigeonSurfaceChecks.ps1');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=args.attach)
profile=out/'profile';profile.mkdir(exist_ok=args.attach)
c=Client(profile);r={'checks':[],'passed':False}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],(op,kw);return s
def check(ok,label):
 r['checks'].append({'passed':bool(ok),'name':label});print(label,ok,flush=True)
 if not args.baseline:assert ok,label
def actors(tag):return cmd('actors',filter=tag)['actors']
def shot(name):cmd('shot',name=name);time.sleep(.5)
try:
 cmd('state');cmd('start')
 for _ in range(120):
  if not cmd('state')['opening']:break
  time.sleep(.2)
 spec=json.loads((root/'Saved/Validation/PigeonSurface/authoring.json').read_text())
 eye=spec['eye'];clue=spec['clue'];inward=spec['inward']
 cmd('gm_collectible',item=3);time.sleep(1)
 r['gm_state']=cmd('state');shot('gm-pigeon-exterior')
 check(math.dist(r['gm_state']['position'],spec['approach'])<50,'Actual collectible GM jump lands at the exterior approach')
 cmd('pose',eye=eye,target=clue);time.sleep(.5)
 bird=next(x for x in actors('MiningCollectible') if x.get('item')==3 and x['position'][2]>0)
 clods=actors('BM_PigeonSoilClods')
 check(len(clods)==1 and not clods[0]['hidden'] and clods[0]['triangles']>0,'Exterior transition uses visible mineable clay geometry')
 feathers=actors('CollectionFeather');r['feathers']=feathers;r['bird']=bird
 check(not bird['released'],'Pigeon remains buried before excavation')
 visible=[]
 for feather in feathers:
  if math.dist(feather['position'],clue)>220:continue
  hit=cmd('trace',**{'from':eye,'to':feather['position']})
  if not hit.get('hit',False):visible.append(feather)
 r['visible_feathers']=visible
 check(len(visible)>=7,'Seven exterior feathers have an unobstructed approach sightline')
 shot('pigeon-front')
 time.sleep(12)
 remaining={a['name']:a for a in actors('CollectionFeather')}
 check(len(visible)>=7 and all(f['name'] in remaining and math.dist(f['position'],remaining[f['name']]['position'])<.1 for f in visible),'Exterior feathers stay supported after 12 seconds')
 for name,offset in [('left',[-150,-80,55]),('right',[150,80,55])]:
  side=[eye[i]+offset[i] for i in range(3)];cmd('pose',eye=side,target=clue);time.sleep(.5);shot('pigeon-'+name)
 r['energy']=cmd('state')
 check(abs(r['energy']['drill_recovery_per_second']-1.5)<.01,'Base drill regeneration is 1.5 energy per second')
 if not args.baseline:
  cmd('pose',eye=eye,target=clue)
  feather=visible[0];point=[feather['position'][i]+inward[i]*12 for i in range(3)]
  cmd('mine',actor=bird['host'],point=point,radius=48);time.sleep(.25)
  falling=next((f for f in actors('CollectionFeather') if f['name']==feather['name']),None)
  check(falling is not None and 'FeatherFalling' in falling['tags'],'Removing exterior feather support starts falling')
  shot('feather-falling')
  cmd('mine',actor=bird['host'],point=bird['position'],radius=230);time.sleep(.7)
  after=next(x for x in actors('MiningCollectible') if x['name']==bird['name'])
  check(after['released'] and math.dist(after['position'],bird['position'])<.1,'Excavation releases the pigeon at its buried position')
  shot('pigeon-exposed')
  sample=next(s for s in spec['clods'] if s['side']==-1)
  start=[sample['position'][i]+sample['normal'][i]*60 for i in range(3)]
  end=[sample['position'][i]-sample['normal'][i]*5 for i in range(3)]
  before=cmd('trace',**{'from':start,'to':end})
  check(before.get('hit_actor')==clods[0]['name'],'Transition clods occupy real wall-attached geometry')
  cmd('mine',actor=clods[0]['name'],point=sample['position'],radius=sample['width']*1.8);time.sleep(.3)
  after=cmd('trace',**{'from':start,'to':end})
  check(after.get('hit_actor')!=clods[0]['name'],'Transition clods can be excavated without a residual painted patch')
 r['passed']=all(x['passed'] for x in r['checks'])
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit')
 except:pass
if not r['passed'] and not args.baseline:raise SystemExit(1)
