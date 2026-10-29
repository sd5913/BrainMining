"""Verify the tilted TV/stage wall join in an isolated packaged game."""
import json,subprocess,time,traceback,sys
from pathlib import Path
from packaged_qa_client import Client
root=Path(__file__).resolve().parents[1];out=(root/sys.argv[1]).resolve();profile=out/'profile'
assert not profile.exists();out.mkdir(parents=True)
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
process=subprocess.Popen([sys.argv[2],'-windowed','-RenderOffscreen','-unattended','-NoSplash','-MiningQA','-UserDir='+str(profile)],startupinfo=si)
c=Client(profile);r={'passed':False,'checks':[]}
def cmd(op,**kw):
 d=c.command(op,timeout=180,**kw);assert d['ok'],(op,d);return d
def wait(test):
 end=time.monotonic()+120
 while time.monotonic()<end:
  s=cmd('state')
  if test(s):return s
  time.sleep(.2)
 raise AssertionError(s)
try:
 wait(lambda s:bool(s['menu_buttons']));cmd('start');wait(lambda s:not s['opening'])
 cmd('viewport',width=1280,height=720);cmd('gm_tool',tool=0)
 cmd('area_fixture',area=3);cmd('escape_complete_fixture')
 for name,eye,target in [
  ('right',(31100,11750,20100),(30300,13300,20500)),
  ('left',(31100,11750,20100),(30300,10200,20500)),
  ('back',(31500,11750,20200),(29800,11750,20200))]:
  cmd('pose',eye=eye,target=target);time.sleep(1);cmd('shot',name=name);time.sleep(.3)
 for x in [30000,30200,30400,30600]:
  for z in [20000,20300,20600]:
   for side in [-1,1]:
    hit=cmd('trace',capsule=True,**{'from':[x,11750,z],'to':[x,11750+side*4000,z]})
    assert hit['hit'],(x,z,side,hit)
 r['checks'].append('All 24 side-wall capsule sweeps are blocked at the TV transition')
 for a,b in [([30000,11750,20300],[30700,11750,20300]),([31100,11750,20010],[32500,11750,20010])]:
  hit=cmd('trace',capsule=True,**{'from':a,'to':b});assert not hit['hit'],hit
 r['checks'].append('TV exit and stage walking route remain open')
 cmd('area_fixture',area=5);cmd('ending_entry_fixture');wait(lambda s:s['map']=='Lvl_EndingEmergence_Demo')
 cmd('film_fixture');time.sleep(2);before=cmd('state');r['ending_before']=before.get('ending_shot')
 cmd('return_menu');wait(lambda s:s['opening'] and bool(s['menu_buttons']))
 cmd('continue');after=wait(lambda s:s['map']=='Lvl_EndingEmergence_Demo');r['ending_after']=after.get('ending_shot')
 assert after['saved_checkpoint']==5 and after['ending_gate']==0
 r['checks'].append('Interrupting the later ending film resumes the opening room')
 r['passed']=True
except Exception:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 if process.poll() is None:
  try:cmd('quit');process.wait(60)
  except Exception:process.terminate();process.wait(15)
 (out/'report.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(r,ensure_ascii=False),flush=True)
if not r['passed']:raise SystemExit(1)
