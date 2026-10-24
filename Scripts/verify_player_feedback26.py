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
 wait(lambda s:s['opening']);cmd('start');wait(lambda s:not s['opening']);time.sleep(1);shot('start-room')
 for tool in [0,1,2]:cmd('gm_tool',tool=tool)
 cmd('gm_jump',area=0);time.sleep(.5)
 for typ,n,Id in [('Copper',25,12),('Iron',15,13),('Malachite',10,14),('Diamond',5,15)]:
  region=1 if Id<14 else Id-12;cmd('gm_jump',area=(region-1)*2)
  mineral={'Copper':'2','Iron':'4','Malachite':'6','Diamond':'9'}[typ]
  cmd('mineral_fixture',mineral=mineral,amount=n-1)
  cmd('mineral_progress_fixture',mineral=int(mineral),amount=n-1)
  ore=next(x for x in cmd('actors',filter=f'BM_Campaign_MainGame{region:02}')['actors'] if x.get('mineral')==mineral and not x['collected'])
  point=[0,0,30000];cmd('mineral_pickup_fixture',actor=ore['path'],point=point)
  cmd('pose',eye=[-110,0,30020],target=point);time.sleep(.3);cmd('aim',target=point);cmd('treasure')
  time.sleep(.6);s=cmd('state');r.setdefault('reward_states',[]).append(s);shot('reward-'+typ)
  wait(lambda s:s['collection_counts'][Id]>0 and s['pickup_id']==-1)
 s=cmd('state');totals=s['mineral_totals'];cmd('upgrade');s=cmd('state');check(s['mineral_totals']==totals and s['copper']<totals[0],'Upgrade spends inventory but preserves cumulative totals');shot('ore-total-and-upgrade')
 time.sleep(2);c.key('C');s=cmd('state');check(any(x['playing'] and x['unpaused'] for x in s['music']),'Catalogue music stays playing while game is paused')
 for Id in [12,13,14,15,3,0,8,17,19,21,22]:
  cmd('catalogue_owned_fixture',item=Id);cmd('catalogue_rotate',item=Id,yaw=0,pitch=0);time.sleep(.7);shot('preview-'+str(Id))
  if Id==3:
   one=cmd('state')['catalogue_motion'];time.sleep(.4);two=cmd('state')['catalogue_motion'];check(one[1:]==two[1:],'Inspection pigeon wings remain still')
  if Id==14:r['catalogue_description']=cmd('state')['catalogue_description']
 c.key('C');s=cmd('state');check(not any(x['unpaused'] for x in s['music']),'Closing catalogue restores normal music pause policy')
 for area in [0,2,4]:
  cmd('gm_jump',area=area);time.sleep(.8);s=cmd('state');check(all(x['visible'] and x['intensity']>0 for x in s['lamps'] if x['name'] in ['MiningLight','MiningFillLight']),f'Mining area {area} headlamps enabled');c.key('V');time.sleep(.3);check(cmd('state')['tool_audio_loops']>0,f'Drill audio starts in area {area}');c.key('V');time.sleep(.3);check(cmd('state')['tool_audio_loops']==0,f'Drill audio stops in area {area}')
 data=json.loads((root/'Saved/Validation/PlayerFeedback26/authoring-before.json').read_text(encoding='utf8'))
 toilet=next(p for p in data['placements'] if p['id']==0);front=toilet['clue'];eye=plus(front,[-250,0,120]);cmd('pose',eye=eye,target=plus(front,[150,0,200]));time.sleep(.7);shot('heaven-approach')
 rays=[]
 for dz in [0,300,700]:
  for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
   target=plus(eye,[dx*1200,dy*1200,dz]);hit=cmd('trace',**{'from':eye,'to':target});rays.append(hit)
 r['heaven_rays']=rays
 cmd('gm_jump',area=5);time.sleep(.5);museum=cmd('actors',filter='WenSiMirrorPuzzle')['actors'][0]['position'];target=plus(museum,[1900,120,140]);cmd('pose',eye=plus(target,[-320,-120,95]),target=target);time.sleep(1);shot('museum-hopper')
 cmd('gm_jump',area=4);cmd('save');cmd('return_menu');wait(lambda s:s['opening']);s=cmd('state');check(s['has_save'],'Return to menu completes saving and map reload');cmd('continue');wait(lambda s:not s['opening']);s=cmd('state');check(s['mineral_totals']==totals,'Continue restores cumulative mineral totals')
 r['passed']=True
except:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');proc.wait(timeout=30)
 except:proc.terminate()
if not r['passed']:raise SystemExit(1)
