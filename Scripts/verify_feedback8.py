"""Fresh-start audio, pause/catalogue lifecycle and localized scene regression fixtures."""
import json,subprocess,time,traceback,wave,array,math,argparse
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe',help='Verify the packaged Windows executable');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);profile=out/'profile';assert not profile.exists()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
p=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'passed':False,'checks':[],'states':{},'audio':{}}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],(op,s);return s
def check(ok,label):
 r['checks'].append({'passed':bool(ok),'label':label});assert ok,label;print('PASS',label,flush=True)
def state(label):s=cmd('state');r['states'][label]=s;return s
def music(s):return next(x for x in s['music'] if x['name']=='M_DreamyFlashback_Loop')
def capture(label):
 cmd('audio_capture_start');time.sleep(3);cmd('audio_capture_stop',name=label);path=c.directory/(label+'.wav')
 end=time.monotonic()+15
 while not path.exists() and time.monotonic()<end:time.sleep(.1)
 with wave.open(str(path),'rb') as w:samples=array.array('h',w.readframes(w.getnframes()))
 rms=math.sqrt(sum(float(v)*v for v in samples)/len(samples))/32768;r['audio'][label]=rms;check(rms>.0001,label+' has real audio output')
def shot(name):cmd('shot',name=name);time.sleep(.5)
def add(p,q):return [p[i]+q[i] for i in range(3)]
try:
 cmd('state');cmd('start');time.sleep(3);s=state('fresh-start');first=music(s)['player_id']
 check(music(s)['gain']>.119 and music(s)['unpaused'],'Fresh start uses full exploration gain and a pause-independent music voice')
 capture('fresh-start')
 cmd('pause');time.sleep(.8);s=state('escape-menu');check(abs(s['menu_music_gain']-.35)<.001,'ESC ducks music to 35 percent');capture('escape-menu')
 cmd('pause');time.sleep(.8);s=state('resume');check(s['menu_music_gain']==1 and music(s)['player_id']==first,'ESC resumes full volume without restarting');capture('resume')
 for i in range(3):
  c.key('C');time.sleep(.8);s=state('catalogue-'+str(i));check(music(s)['playing'] and music(s)['player_id']==first and s['menu_music_gain']==1,'Catalogue keeps music at normal volume '+str(i))
  if i==0:capture('catalogue-open')
  cmd('catalogue_button',name='CloseButton');time.sleep(.5)
 capture('after-catalogue');cmd('gm_mineral',item=12);time.sleep(5);capture('after-collectible')
 check(music(state('after-collectible'))['player_id']==first,'Collectible sound does not replace exploration music')
 cmd('gm_tool',tool=0)
 cmd('pose',eye=[-30700,0,12180],target=[-30150,0,12140]);time.sleep(.65)
 samples=[]
 for _ in range(12):samples.append(cmd('state'));time.sleep(.2)
 check(any('软软的泥土' in s.get('subtitle','') and not s['soil_passed'] for s in samples),'Soft-soil hint plays before crossing or digging');shot('soil-approach')
 soil=next(x for x in cmd('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in x)
 cmd('mine',actor=soil['path'],point=soil['position'],radius=230);time.sleep(2)
 hits=[]
 for x in [-30200,-30150,-30100,-30000]:
  for y in [-150,0,150]:hits.append(cmd('trace',**{'from':[x,y,11980],'to':[x,y,11600]}))
 check(all(x.get('hit_actor')!='DynamicMeshActor_6' for x in hits),'Entrance floor no longer exposes the old bedrock end cap')
 cmd('pose',eye=[-30260,-80,12100],target=[-30060,0,11880]);shot('soil-floor')
 cmd('pose',eye=[-28440,830,10970],target=[-28358,554,10967]);shot('copper-lowered')
 cmd('area_fixture',area=5);time.sleep(3)
 origin=cmd('actors',filter='WenSiMirrorPuzzle')['actors'][0]['position']
 balloon=next(x for x in cmd('actors',filter='WenSiMirrorExhibit')['actors'] if x['kind']==3)
 # Offset by 8 cm from the actual 1.5 cm string; the physical string stays thin.
 target=add(origin,[-450,-342,150]);eye=add(target,[-210,0,0]);cmd('pose',eye=eye,target=target);time.sleep(.5)
 check(cmd('state')['museum_aim_kind']==3,'Balloon string can be selected with an 8 cm aim margin');shot('balloon-selection')
 cmd('museum_word_fixture',kind=9,word=6);time.sleep(3)
 ball=next(x for x in cmd('actors',filter='WenSiMirrorExhibit')['actors'] if x['kind']==9)
 check(abs(ball['position'][2]-(origin[2]+80))<1,'Small ball settles at the existing circuit contact height')
 cmd('pose',eye=add(origin,[1660,-40,135]),target=add(origin,[1900,120,75]));shot('ball-contact-small')
 cmd('museum_word_fixture',kind=9,word=4);time.sleep(1);shot('ball-contact-conductive')
 check('power=1' in cmd('state').get('museum',''),'Conductive landed ball still powers the circuit')
 r['passed']=True
except Exception:r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 try:cmd('quit');p.wait(timeout=30)
 except Exception:p.terminate();p.wait(timeout=20)
if not r['passed']:raise SystemExit(1)
