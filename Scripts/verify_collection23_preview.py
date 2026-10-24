"""Real game rendering in an isolated profile; no desktop input or recordings."""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('output');p.add_argument('--exe');a=p.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists();out.mkdir(parents=True,exist_ok=True)
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-noraytracing','-DisablePlugins=AllToolsets,ModelContextProtocol']
proc=subprocess.Popen(launch+['-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-unattended','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')]);c=Client(profile);report={'passed':False,'checks':[]}
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,kw);return r
def wait(pred):
 end=time.monotonic()+180
 while time.monotonic()<end:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.2)
 raise AssertionError(s)
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def shot(name):cmd('shot',name=name);time.sleep(.35)
try:
 wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening'])
 for pickup in cmd('actors',filter='StarterEquipment')['actors']:
  cmd('pose',actor=pickup['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
 cmd('pose',eye=[-30685,-10,12125],target=[-30150,0,12180]);time.sleep(.8);shot('entrance-soil-seam')
 c.key('C');time.sleep(.6)
 for item in [0,12,13,14,15,16,22]:
  cmd('catalogue_rotate',item=item,yaw=0,pitch=0);s=cmd('state');labels={v['name']:v['text'] for v in s['catalogue_labels']}
  check(labels['ItemName']=='尚未发现',f'Locked {item} hides its real name')
  check(not s['catalogue_preview_visible'],f'Locked {item} hides its model')
  check('升级消耗' not in s['catalogue_description'],f'Locked {item} has no redundant note')
  if item in [0,16,22]:check('继续探索，寻找潜藏的灵感吧！' in s['catalogue_description'],f'Locked {item} has short hint')
  if item==12:
   # Use page controls to reach the same item so the cards refresh to page four.
   for _ in range(3):cmd('catalogue_button',name='NextButton')
   s=cmd('state');report['mineral_page']=s['catalogue_labels'];report['buttons']=s['catalogue_buttons'];shot('locked-mineral-page')
  if item==0:shot('locked-normal')
 cmd('catalogue_hover',name='CloseButton',hover=True);time.sleep(.2);s=cmd('state');back=next(v for v in s['catalogue_buttons'] if v['name']=='CloseButton');check(back['hovered'] and back['scale']>1,'Back button visibly grows on hover');shot('back-hover');cmd('catalogue_hover',name='CloseButton',hover=False)
 for item in range(23):
  cmd('catalogue_owned_fixture',item=item);time.sleep(.5);s1=cmd('state');time.sleep(.35);s2=cmd('state');check(s1['catalogue_motion']!=s2['catalogue_motion'],f'Item {item} animates in actual preview')
  if item>=16 or item in [9,10]:shot('inspect-'+str(item))
 check(cmd('state')['catalogue_count']=='已收集23/23','Catalogue includes all 23 items')
 shot('all23-counter');cmd('catalogue_button',name='CloseButton');report['passed']=True
except Exception:
 import traceback;report['error']=traceback.format_exc();raise
finally:
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 if proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=60)
  except Exception:pass
