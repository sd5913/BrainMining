"""Real menu/relaunch recovery and input, with named traversal-only fixtures."""
import argparse,json,re,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('output');p.add_argument('--exe',required=True);p.add_argument('--editor',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=(root/a.output).resolve();profile=out/'profile'
assert not profile.exists();profile.mkdir(parents=True)
c=Client(profile);report={'passed':False,'checks':[]};process=None
def launch():
 global process
 si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
 extra=[str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game'] if a.editor else []
 process=subprocess.Popen([a.exe,*extra,'-windowed','-ResX=1024','-ResY=768','-RenderOffscreen','-unattended','-NoSplash','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,r);return r
def wait(predicate,seconds=120):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=cmd('state')
  if predicate(s):return s
  time.sleep(.15)
 raise AssertionError('Timeout: '+str(s))
def check(v,label):
 assert v,label
 report['checks'].append(label);print('PASS '+label,flush=True)
def menu_resume():
 cmd('return_menu');wait(lambda s:s['opening'] and bool(s['menu_buttons']))
 cmd('continue');return wait(lambda s:not s['opening'])
def shot(name):cmd('shot',name=name);time.sleep(.3)
def round_play(n):
 s=wait(lambda s:s['card_phase']==4 and s['card_round']==n and bool(s['interaction_action']))
 check('全选' in s['interaction_action'] or 'Select' in s['interaction_action'],'Round %d select-all hint'%n)
 cmd('slate_key',key='SpaceBar')
 s=cmd('state');check(s['card_selected']==len(s['card_hand']) and s['card_phase']==4,'Round %d Space selects without playing'%n)
 if n==4:shot('17-paper-cut-cards-4by3')
 cmd('slate_key',key='SpaceBar')
 if n==4:
  time.sleep(.8);shot('wrapped-fusion-4by3')
 wait(lambda s:s['card_phase'] in (4,6))
try:
 launch();wait(lambda s:bool(s['menu_buttons']));cmd('start');wait(lambda s:not s['opening'])
 cmd('viewport',width=1024,height=768);time.sleep(.5)
 cmd('gm_tool',tool=0)
 cmd('area_fixture',area=3)
 echo=cmd('actors',filter='EchoPassageStage')['actors'][0]['position']
 cmd('pose',eye=[echo[0]+600,echo[1],echo[2]+150],target=[echo[0]+1000,echo[1],echo[2]+100])
 cmd('echo_progress_fixture');time.sleep(.5);cmd('save');before=cmd('state')
 menu_resume();after=wait(lambda s:s['echo_landed'])
 check(after['echo_knocks']==7 and after['echo_collapsed'],'Blind passage retains knock progress')
 check(sum((x-y)**2 for x,y in zip(before['position'],after['position']))<400,'Blind passage retains position')
 stage=cmd('actors',filter='CardRoomStage')['actors'][0]['position']
 cmd('pose',eye=[stage[0]-75,stage[1],stage[2]+115],target=stage);cmd('card',action='interact')
 wait(lambda s:s.get('saved_checkpoint')==3)
 round_play(1);round_play(2)
 wait(lambda s:s['card_round']==3 and s['card_phase']==4)
 menu_resume();wait(lambda s:s['card_round']==1 and s['card_phase']==4)
 check(True,'Round three exit resumes at round one through the main menu')
 for n in range(1,5):round_play(n)
 wait(lambda s:s.get('saved_checkpoint')==4)
 cmd('card',action='interact');check(cmd('state')['drill'],'Drill claimed before rollback')
 menu_resume();s=wait(lambda s:s['card_phase']==6)
 check(not s['drill'] and s['journey_phase']==0,'Unfinished escape resumes at drill waiting for pickup')
 # A full process restart verifies disk persistence, not a cached in-memory snapshot.
 cmd('quit');process.wait(60);launch();wait(lambda s:bool(s['menu_buttons']));cmd('continue')
 s=wait(lambda s:not s['opening'] and s['card_phase']==6)
 check(not s['drill'],'Drill checkpoint survives process restart')
 cmd('legacy_duel_save_fixture');menu_resume();s=wait(lambda s:s['card_phase']==6)
 check(not s['drill'],'Legacy duel save with unstable runtime actor name resumes successfully')
 cmd('card',action='interact');cmd('escape_complete_fixture')
 exit=cmd('state')['journey_exit'];cmd('pose',eye=[exit[0],exit[1],exit[2]+70],target=[exit[0]+1000,exit[1],exit[2]+70])
 wait(lambda s:s.get('saved_checkpoint')==0 and s.get('saved_card_state')==3)
 menu_resume();s=wait(lambda s:s['journey_phase']==4)
 check(s['drill'] and s['card_phase']==7,'Completed parkour remains completed')
 shot('stage-approach')
 for name,eye,target in [
  ('stage-door-front',(32800,11650,20080),(33200,11750,20100)),
  ('stage-floor-back',(33000,11650,20065),(31900,11650,19900)),
  ('stage-door-roof',(32900,11650,20070),(33120,12000,20250))]:
  cmd('pose',eye=eye,target=target);time.sleep(.6);shot(name)
 cmd('area_fixture',area=1);cmd('office');wait(lambda s:s.get('saved_checkpoint')==1)
 time.sleep(8);menu_resume();s=wait(lambda s:'phase=1' in s['office'])
 check(float(re.search(r'elapsed=([\d.]+)',s['office']).group(1))<5,'Office work restarts at gameplay start')
 wait(lambda s:s.get('saved_checkpoint')==2,seconds=110)
 menu_resume();s=wait(lambda s:'Exit phase=2' in s['office_exit'])
 check('phase=3' in s['office'] and not s['bomb'],'Office resumes with bomb offered, without repeating popups')
 cmd('area_fixture',area=5);cmd('ending_entry_fixture')
 wait(lambda s:s['map']=='Lvl_EndingEmergence_Demo')
 time.sleep(5);cmd('return_menu');wait(lambda s:s['opening'] and bool(s['menu_buttons']))
 cmd('continue');s=wait(lambda s:s['map']=='Lvl_EndingEmergence_Demo')
 check(s['saved_checkpoint']==5 and s['ending_gate']==0,'Ending resumes at its start across map travel')
 report['passed']=True
except Exception:
 report['error']=traceback.format_exc();print(report['error'],flush=True)
finally:
 if process and process.poll() is None:
  try:cmd('quit');process.wait(60)
  except Exception:process.terminate();process.wait(15)
 (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if not report['passed']:raise SystemExit(1)
