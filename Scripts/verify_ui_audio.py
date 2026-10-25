"""Exercise real Slate hover/click events and record their actual mixer output."""
import argparse, array, json, math, subprocess, time, traceback, wave
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
profile=out/'profile';assert not profile.exists()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),
 '/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures',
 '-DisablePlugins=AllToolsets,ModelContextProtocol']
p=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA',
 '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'passed':False,'checks':[],'audio':{}}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],(op,s.get('error'));return s
def check(ok,label):
 r['checks'].append(dict(passed=bool(ok),label=label));print(('PASS ' if ok else 'FAIL ')+label,flush=True);assert ok,label
def volume(channel,value):cmd('audio_volume_fixture',channel=channel,value=value);time.sleep(.35)
def music():return next(x for x in cmd('state')['music'] if x['name']=='M_DreamyFlashback_Loop')
def capture(label,action=None):
 cmd('audio_capture_start');time.sleep(.2)
 if action:action()
 time.sleep(.8);cmd('audio_capture_stop',name=label)
 path=c.directory/(label+'.wav');deadline=time.monotonic()+15
 while not path.exists() and time.monotonic()<deadline:time.sleep(.1)
 with wave.open(str(path),'rb') as w:
  rate=w.getframerate();channels=w.getnchannels();samples=array.array('h',w.readframes(w.getnframes()))
 rms=math.sqrt(sum(float(v)*v for v in samples)/len(samples))/32768
 active=sum(abs(v)>10 for v in samples)/channels/rate
 r['audio'][label]=dict(rms=rms,active_seconds=active,peak=max(map(abs,samples))/32768)
 return rms
try:
 cmd('state');time.sleep(3);first=music()['player_id']
 for ch in [1,3,4]:volume(ch,0)
 baseline=capture('title-idle')
 hover=capture('title-hover',lambda:cmd('menu_pointer',action='start',phase='move'))
 check(hover>max(.0001,baseline*2),'Opening menu hover produces real audio')
 hold=capture('title-hover-held');check(hold<hover*.15,'Holding hover does not repeat the sound')
 capture('title-click',lambda:(cmd('menu_pointer',action='start',phase='down'),cmd('menu_pointer',action='start',phase='up')))
 time.sleep(3);cmd('gm_tool',tool=0);c.key('C');time.sleep(1)
 buttons=cmd('state')['catalogue_buttons']
 check(all(b['hover_sound']=='S_UI_Hover' and b['click_sound']=='S_UI_Click' for b in buttons),'Cards, arrows, close and page indicators share the UI sound pair')
 hover=capture('page-hover',lambda:cmd('catalogue_hover',name='PageButton4',hover=True))
 check(hover>.0001,'Page indicator hover is audible')
 check(capture('page-hover-held')<hover*.15,'Page hover stays silent after its initial tap')
 cmd('catalogue_hover',name='PageButton4',hover=False)
 click=capture('page-click',lambda:cmd('catalogue_button',name='PageButton4'))
 check(click>.0001 and r['audio']['page-click']['active_seconds']<.15,'Page activation plays one short click')
 volume(2,0)
 muted=capture('effects-muted',lambda:cmd('catalogue_button',name='PageButton0'))
 check(muted<click*.05,'Effects volume also controls UI audio')
 volume(2,.85);cmd('catalogue_button',name='CloseButton');cmd('pause');time.sleep(.6)
 hover=capture('pause-hover',lambda:cmd('slate_pointer',tag='PauseAction_设置',phase='move'))
 check(hover>.0001,'Hover remains audible while ESC pauses the world')
 click=capture('pause-resume-click',lambda:cmd('slate_activate',tag='PauseAction_继续游戏'))
 check(click>.0001,'Resume click remains audible across closing the pause menu')
 volume(1,1);volume(3,.8);volume(4,1);time.sleep(.7)
 check(music()['playing'] and music()['player_id']==first,'UI interactions never stop or replace the BGM voice')
 check(capture('bgm-restored')>.0001,'BGM remains audible after the UI audio checks')
 r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');p.wait(timeout=30)
 except:p.terminate();p.wait(timeout=20)
if not r['passed']:raise SystemExit(1)
