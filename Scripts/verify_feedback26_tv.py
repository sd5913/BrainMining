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
 wait(lambda s:s['opening']);cmd('start');wait(lambda s:not s['opening']);cmd('gm_tool',tool=0);cmd('gm_jump',area=3);time.sleep(1)
 check(cmd('state')['prototypes']<2,'Prototype two is not completed on entering echo passage')
 echo=cmd('actors',filter='EchoPassageStage')['actors'][0]['position']
 cmd('pose',eye=plus(echo,[0,0,1100]),target=plus(echo,[500,0,700]));cmd('walking_fixture')
 s=wait(lambda s:s['prototypes']==2,20);r['landing_subtitle']=s['subtitle'];shot('echo-landing')
 check('声音' in s['subtitle'],'Prototype two completes with landing sound subtitle')
 stage=cmd('actors',filter='CardRoomStage')['actors'][0]['position'];cmd('pose',eye=plus(stage,[-75,0,115]),target=stage);time.sleep(1)
 check(cmd('state')['prototypes']==3,'Prototype three completes at card room entrance');shot('card-entry')
 cmd('card',action='interact');time.sleep(.5);cmd('card',action='interact');time.sleep(5)
 for turn in range(1,5):
  s=cmd('state');check(s['card_round']==turn and s['card_phase']==4,f'Card round {turn} ready');cmd('card',action='all');cmd('card',action='confirm')
  if turn==2:
   lines=[];end=time.monotonic()+5.2
   while time.monotonic()<end:
    s=cmd('state');lines.append(s['subtitle']);time.sleep(.3)
   r['round_two_subtitles']=lines
  else:time.sleep(5.2)
 check(cmd('state')['card_phase'] in (6,7),'Four card rounds reach victory');shot('drill-reward');cmd('card',action='interact');s=cmd('state');check(s['drill'],'Drill rewarded')
 checkpoint=s['tv_checkpoint_position'];cmd('pose',eye=plus(checkpoint,[0,0,64]),target=s['tv_entry']);time.sleep(6);s=cmd('state');check(s['tv_checkpoint'],'TV checkpoint reached');shot('tv-waypoint')
 rocks={x['path']:x for x in cmd('actors',filter='DiggableRock')['actors']}
 for index,path in enumerate(s['computer_rocks']):
  rock=rocks[path];cmd('mine',actor=path,point=rock['center'],radius=500);time.sleep(.6)
  pos=plus(s['tv_entry'],[250,0,133]) if index==0 else [rock['position'][i]+rock['forward'][i]*(rock['rock_size'][0]/2+100) for i in range(3)]
  cmd('pose',eye=plus(pos,[0,0,64]),target=plus(pos,[500,0,300]));time.sleep(.6);check(cmd('state')['nodes']==index+1,f'TV node {index+1} crossed after excavation')
  if index==0:
   check(cmd('state')['inside_computer'],'Player inside television')
   if not any('Drill' in x for x in cmd('state')['visible_tools']):c.key('V')
   time.sleep(.4);check(cmd('state')['tool_audio_loops']>0,'Drill audio starts inside TV');cmd('key',key='LeftMouseButton',down=True);time.sleep(6.3);cmd('key',key='LeftMouseButton',down=False)
   z=cmd('state');check(z['drill_energy']<20 and z['tool_audio_loops']==0,'Exhausted drill retracts and all drill loops stop inside TV');time.sleep(2);check(cmd('state')['tool_audio_loops']==0,'No stale drill loop while idle with pickaxe');c.key('V');time.sleep(.4);check(cmd('state')['tool_audio_loops']>0,'Recharged drill audio restarts inside TV');c.key('V')
 exit=s['journey_exit'];cmd('pose',eye=plus(exit,[0,0,64]),target=plus(exit,[500,0,64]));time.sleep(1);check(cmd('state')['journey_phase']==4,'TV escape completed')
 cmd('gm_jump',area=4);time.sleep(2);c.key('V');time.sleep(.4);check(cmd('state')['tool_audio_loops']>0,'Drill audio works after TV escape');c.key('V');time.sleep(.4);check(cmd('state')['tool_audio_loops']==0,'Drill audio stops after TV escape')
 r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');proc.wait(timeout=30)
 except:proc.terminate()
if not r['passed']:raise SystemExit(1)
