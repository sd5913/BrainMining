"""Museum equipment policy and authored branch clue/pickup regression."""
import argparse,json,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('output');p.add_argument('--exe');p.add_argument('--fov',type=int,default=100,choices=range(80,116));a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists();profile.mkdir(parents=True)
(profile/'Saved/Config').mkdir(parents=True);(profile/'Saved/Config/BrainMiningAudio.ini').write_text('[BrainMining.Controls]\nFieldOfView='+str(a.fov)+'\n',encoding='utf8')
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
proc=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],cwd=str(Path(launch[0]).parent),startupinfo=startup)
c=Client(profile);report={'passed':False,'checks':[]}
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,kw);return r
def wait(pred):
 until=time.monotonic()+180
 while time.monotonic()<until:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.2)
 raise AssertionError(s)
def check(ok,text):
 assert ok,text
 report['checks'].append(text)
def shot(name):cmd('shot',name=name);time.sleep(.4)
def item(Id):return next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==Id and x['position'][2]>0)
try:
 wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening'])
 for tool in (0,1,2):cmd('gm_tool',tool=tool)
 cmd('gm_jump',area=4);time.sleep(1)
 c.key('V');time.sleep(.5);check(any('Drill' in t for t in cmd('state')['visible_tools']),'Drill visible in mining passage before museum')
 shot('drill-idle')
 cmd('key',key='LeftMouseButton',down=True);time.sleep(.7);shot('drill-running');cmd('key',key='LeftMouseButton',down=False)
 c.key('V');time.sleep(.5);c.key('V');time.sleep(.12);shot('drill-equipping');time.sleep(.5)
 cmd('gm_jump',area=5);time.sleep(1);check(not cmd('state')['visible_tools'],'Museum entry hides every held tool')
 for key in ['G','V']:
  c.key(key);time.sleep(.3);check(not cmd('state')['visible_tools'],'Museum keeps tools hidden after '+key)
 mirror=next(x for x in cmd('actors',filter='WenSiMirrorExhibit')['actors'] if x.get('kind')==10)
 pos=mirror['position'];n=mirror['forward'];cmd('pose',eye=[pos[i]+n[i]*300 for i in range(3)],target=pos);time.sleep(2);shot('museum-miner-lamp')
 parts=cmd('state')['mirror_presence_parts'];check('MinerHelmetDome' in parts and 'MinerLampLens' in parts and not any('Pickaxe' in v for v in parts),'Mirror presence wears lamp helmet without pickaxe')
 cmd('gm_jump',area=4);time.sleep(.6);check(bool(cmd('state')['visible_tools']),'Selected tool returns after leaving museum')
 for spec in json.loads((root/'Config/BranchCollectiblePlacements.json').read_text(encoding='utf8')):
  Id=spec['id'];cmd('gm_jump',area=(spec['region']-1)*2);time.sleep(.3)
  front=spec['clue'];n=spec['normal'];cmd('pose',eye=[front[i]+n[i]*260 for i in range(3)],target=front);time.sleep(1)
  clues=cmd('actors',filter='BranchClue_'+str(Id))['actors'];check(bool(clues),f'{Id} has its own soil clue');check(spec['min_mineral_distance']>=500,f'{Id} separated from mineral cores and fragments')
  shot('clue-'+str(Id))
  if Id==16:
   time.sleep(1.3);shot('steam-later-1');time.sleep(1.7);shot('steam-later-2')
  obj=item(Id);check(not obj['released'],f'{Id} stays buried before excavation')
  if Id==21:check(obj['extent'][1]>100 or obj['extent'][0]>100,'Subway enlarged in the world')
  for depth in [0,90,180,260]:
   cmd('mine',actor=obj['host'],point=[front[i]-n[i]*depth for i in range(3)],radius=195);time.sleep(.3)
  time.sleep(.8);obj=item(Id);check(obj['released'],f'{Id} releases after soil removal')
  check(len(cmd('actors',filter='BranchClue_'+str(Id))['actors'])<len(clues),f'{Id} clue disappears with excavated support')
  q=obj['position'];cmd('pose',eye=[q[i]+n[i]*155+(20 if i==2 else 0) for i in range(3)],target=q);time.sleep(.5);shot('released-'+str(Id))
  # The released collectible can still be falling while a screenshot is captured.
  # Re-aim at its current physical position; never move it or unlock it through GM.
  for attempt in range(6):
   q=item(Id)['position'];cmd('pose',eye=[q[i]+n[i]*145+(20 if i==2 else 0) for i in range(3)],target=q);time.sleep(.12)
   if c.command('collect')['ok']:break
  else:raise AssertionError(f'{Id} cannot be collected by aiming')
  wait(lambda s:s['pickup_id']==-1 and s['collection_counts'][Id]==1)
  check(True,f'{Id} collected through normal aim/E interaction')
 c.key('C');cmd('catalogue_rotate',item=22,yaw=0,pitch=0);time.sleep(.7);shot('phone-renamed');c.key('C')
 for spec in json.loads((root/'Config/ContourBoundaryChecks.json').read_text(encoding='utf8')):
  cmd('gm_jump',area=(spec['region']-1)*2);p=spec['point'];n=spec['normal']
  cmd('pose',eye=[p[i]+n[i]*400 for i in range(3)],target=p);time.sleep(.5)
  hit=cmd('trace',**{'from':[p[i]+n[i]*450 for i in range(3)],'to':[p[i]-n[i]*150 for i in range(3)]})
  check(hit.get('hit_actor')==spec['shell'] and not hit['diggable'],f"Region {spec['region']} contour bedrock blocks collision and is not diggable")
  cmd('mine',actor=spec['buffer'],point=[p[i]-n[i]*80 for i in range(3)],radius=220);time.sleep(.5)
  cmd('pose',eye=[p[i]-n[i]*70 for i in range(3)],target=[p[i]+n[i]*300 for i in range(3)]);time.sleep(.5)
  hit=cmd('trace',**{'from':[p[i]-n[i]*70 for i in range(3)],'to':[p[i]+n[i]*450 for i in range(3)]})
  check(hit.get('hit_actor')==spec['shell'],f"Region {spec['region']} extra soil excavates up to the intact boundary")
  hit=cmd('trace',capsule=True,**{'from':[p[i]-n[i]*70 for i in range(3)],'to':[p[i]+n[i]*450 for i in range(3)]})
  check(hit.get('hit_actor')==spec['shell'],f"Region {spec['region']} boundary blocks a player-sized capsule from inside")
  shot('boundary-'+str(spec['region']))
 cmd('save');wait(lambda s:not s['save_in_flight']);cmd('quit');proc.wait(timeout=60)
 proc=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'reload.log')],cwd=str(Path(launch[0]).parent),startupinfo=startup)
 wait(lambda s:s['opening'] and s['menu_buttons']);cmd('continue');s=wait(lambda s:not s['opening'])
 check(all(s['collection_counts'][i]==1 for i in range(16,23)),'Reload preserves all seven relocated collectibles')
 for spec in json.loads((root/'Config/ContourBoundaryChecks.json').read_text(encoding='utf8')):
  cmd('gm_jump',area=(spec['region']-1)*2);time.sleep(.5);p=spec['point'];n=spec['normal']
  hit=cmd('trace',**{'from':[p[i]-n[i]*70 for i in range(3)],'to':[p[i]+n[i]*450 for i in range(3)]})
  check(hit.get('hit_actor')==spec['shell'],f"Region {spec['region']} added soil excavation survives save/reload")
 report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 if proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=60)
  except Exception:pass
