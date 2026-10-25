"""Inspect the real collection UI and initial music fade with an isolated save."""
import argparse,json,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
profile=out/'profile';assert not profile.exists()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
p=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'passed':False,'checks':[]}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],(op,s.get('error'));return s
def check(ok,label):
 r['checks'].append(dict(passed=bool(ok),label=label));assert ok,label;print('PASS',label,flush=True)
def music(s):return next((x for x in s['music'] if x['name']=='M_DreamyFlashback_Loop'),None)
def labels(s):return {x['name']:x['text'] for x in s['catalogue_labels']}
def shot(name):time.sleep(.3);cmd('shot',name=name);time.sleep(.4)
try:
 s=cmd('state');samples=[];deadline=time.monotonic()+5
 while time.monotonic()<deadline:
  m=music(s)
  if m:samples.append(m)
  if samples and not m['fading_in']:break
  s=cmd('state')
 r['initial_music']=samples
 check(any(m['fading_in'] for m in samples),'Title music begins with audio-thread fade in')
 time.sleep(3);m=music(cmd('state'));first=m['player_id']
 check(m['playing'] and not m['fading_in'] and m['gain']>.119,'Title music reaches normal playback')
 cmd('start');time.sleep(3);cmd('gm_tool',tool=0);c.key('C');time.sleep(.7)
 s=cmd('state')
 for i,text in enumerate(['收集25个铜矿','收集15个铁矿','收集10个孔雀石','收集5个钻石']):check(labels(s).get('ItemCondition'+str(i))==text,'Locked '+text)
 shot('first-page-locked')
 for i in [12,13,14,15]:cmd('catalogue_owned_fixture',item=i)
 s=cmd('state')
 for i,text in enumerate(['已收集25个铜矿','已收集15个铁矿','已收集10个孔雀石','已收集5个钻石']):check(labels(s).get('ItemCondition'+str(i))==text,'Owned '+text)
 cmd('catalogue_hover',name='NextButton',hover=True);shot('first-page-owned-hover')
 check(next(b for b in cmd('state')['catalogue_buttons'] if b['name']=='NextButton')['hovered'],'Next arrow accepts hover')
 cmd('catalogue_hover',name='NextButton',hover=False)
 selections=[]
 for page in range(1,6):
  cmd('catalogue_button',name='NextButton');time.sleep(.2);selections.append(cmd('state')['selected_item']);shot('page-'+str(page+1))
 check(len(set(selections))==5,'All six pages can be visited')
 cmd('catalogue_hover',name='PreviousButton',hover=True);shot('last-page-previous-hover')
 check(next(b for b in cmd('state')['catalogue_buttons'] if b['name']=='PreviousButton')['hovered'],'Previous arrow accepts hover')
 cmd('catalogue_hover',name='PreviousButton',hover=False)
 for _ in range(5):cmd('catalogue_button',name='PreviousButton')
 check(cmd('state')['selected_item']==12,'Returning to first page selects first mineral collectible')
 cmd('catalogue_button',name='CloseButton');cmd('pause');time.sleep(.6);s=cmd('state')
 check(music(s)['player_id']==first and abs(s['menu_music_gain']-.35)<.001,'ESC retains music and ducks volume')
 cmd('pause');time.sleep(.6);s=cmd('state')
 check(music(s)['player_id']==first and s['menu_music_gain']==1,'Resume keeps the same music voice')
 r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');p.wait(timeout=30)
 except:p.terminate();p.wait(timeout=20)
if not r['passed']:raise SystemExit(1)
