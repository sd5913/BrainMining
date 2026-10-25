import argparse,json,subprocess,time,traceback,math
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe',help='Verify a packaged Windows executable');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(args.output).resolve();out.mkdir(parents=True,exist_ok=True);profile=out/'profile';assert not profile.exists()
assets=json.loads((root/'Saved/Validation/Feedback9/assets.json').read_text(encoding='utf8'));inspection=json.loads((root/'Saved/Validation/Feedback9/inspection.json').read_text(encoding='utf8'))
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
launch=[str(Path(args.exe).resolve())] if args.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
p=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'passed':False,'checks':[]}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],(op,s.get('error'));return s
def check(ok,label):r['checks'].append(dict(passed=bool(ok),label=label));assert ok,label;print('PASS',label,flush=True)
def add(a,b,scale=1):return [a[i]+b[i]*scale for i in range(3)]
def shot(name,eye=None,target=None):
 if eye is not None:cmd('pose',eye=eye,target=target)
 time.sleep(.6);cmd('shot',name=name);time.sleep(.4)
try:
 cmd('state');cmd('start');time.sleep(3);s=cmd('state');check(s['drill_capacity']==36 and s['drill_recovery_per_second']==3,'Drill capacity 36 and recovery 3 per second')
 cmd('gm_tool',tool=0)
 c.key('C');time.sleep(.5)
 for i in [12,13,14,15]:cmd('catalogue_owned_fixture',item=i)
 cmd('catalogue_rotate',item=12,yaw=0,pitch=0);time.sleep(.4);s=cmd('state');labels={x['name']:x['text'] for x in s['catalogue_labels']}
 for n,text in enumerate(['已收集25个铜矿','已收集15个铁矿','已收集10个孔雀石','已收集5个钻石']):check(labels.get('ItemCondition'+str(n))==text,'Owned mineral reward retains '+text)
 shot('mineral-conditions');cmd('catalogue_button',name='CloseButton')
 shot('copper-same-view',[-27930,800,10700],[-27072,928,10688])
 pos=assets['copper']['after'];shot('copper-close',[-27230,860,10340],pos)
 treasure=next(a for a in cmd('actors',filter='MiningTreasure')['actors'] if a.get('placement')==9)
 check(treasure['position'][2]<10600,'Correct copper placement moved to lower sidewall')
 cmd('area_fixture',area=2);time.sleep(2)
 coin=next(x for x in inspection['collections'] if x['id']==18);clue=coin['clue'];n=coin['normal'];eye=add(clue,n,-280)
 before=cmd('actors',filter='BranchClue_18')['actors'];r['coin_before']=before
 check(len(before)>=35,'Coins are distributed in multiple supported soil layers')
 shot('coin-before',eye,clue)
 host=coin['rock'].split('.')[-1].split(':')[-1]
 # Match by exact stored object name, rather than a substring such as Rock_5.
 host=coin['rock'].rsplit('.',1)[-1]
 cmd('mine',actor=host,point=add(clue,n,35),radius=135);time.sleep(1.5)
 after=cmd('actors',filter='BranchClue_18')['actors'];r['coin_after']=after
 check(7<len(after)<len(before),'Excavating outer coin layer leaves buried clues further inside')
 check(any(sum((a['position'][i]-clue[i])*n[i] for i in range(3))>200 for a in after),'Coin clues extend over two metres into the soil')
 shot('coin-after',eye,add(clue,n,130))
 bird=next(x for x in inspection['collections'] if x['id']==3);pos=bird['pos'];n=bird['normal'];eye=add(pos,n,280)
 cmd('pose',eye=eye,target=pos)
 for radius in [45,60,80,100]:
  cmd('mine',actor=bird['rock'].rsplit('.',1)[-1],point=add(pos,n,35),radius=radius);time.sleep(.5)
  item=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==3 and not x.get('collected'))
  if item['exposure']>.05:break
 direction=[eye[i]-item['position'][i] for i in range(3)];length=math.sqrt(sum(x*x for x in direction));dot=sum(-item['forward'][i]*direction[i]/length for i in range(3))
 r['bird']=item;check(dot>.9,'First revealed pigeon faces player opening even from opposite side');shot('pigeon-outward')
 cmd('area_fixture',area=4);time.sleep(2)
 for b in assets['bombs']:shot(b['name'],add(b['position'],b['direction'],-420),b['position'])
 r['passed']=True
except: r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');p.wait(timeout=30)
 except:p.terminate();p.wait(timeout=20)
if not r['passed']:raise SystemExit(1)
