"""Exercise actual museum focus and workstation camera travel in an isolated game."""
import argparse,json,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe',help='Verify a packaged Shipping executable instead of the editor');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=(root/args.output).resolve();profile=out/'profile';assert not profile.exists();out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'checks':[],'camera':[]};proc=None

def check(ok,label):
 assert ok,label
 report['checks'].append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
 r=c.command(op,**kw);assert r['ok'],(op,r);return r
def wait(pred,timeout=90):
 until=time.monotonic()+timeout
 while True:
  s=cmd('state')
  if pred(s):return s
  assert time.monotonic()<until,s
  time.sleep(.1)
try:
 for platform in ('Windows','WindowsEditor'):
  config=profile/'Saved/Config'/platform/'GameUserSettings.ini';config.parent.mkdir(parents=True,exist_ok=True)
  config.write_text('[BrainMining.Localization]\nLanguagePreference=zh-Hans\n',encoding='utf8')
 launch=[args.exe] if args.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game']
 editor_flags=[] if args.exe else ['-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
 proc=subprocess.Popen(launch+editor_flags+['-windowed','-ResX=1280','-ResY=800','-RenderOffscreen','-NoSplash','-nosound','-MiningQA','-UserDir='+str(profile)])
 c=Client(profile);c.command('state',timeout=180);wait(lambda s:s['menu_buttons']);cmd('start');wait(lambda s:not s['opening']);time.sleep(2)
 cmd('area_fixture',area=5);time.sleep(3)
 exhibits=cmd('actors',filter='WenSiMirrorExhibit',parts=True)['actors'];stone=next(a for a in exhibits if a['kind']==0);p=stone['position']
 for word,label in [(1,'solid-cover'),(3,'passable-cover')]:
  cmd('museum_word_fixture',kind=1,word=word)
  cmd('pose',eye=[p[0]-160,p[1],p[2]+55],target=stone['center']);time.sleep(1)
  s=cmd('state');check(s['museum_aim_kind']==0,'Stone can be aimed through the open front: '+label)
  check(s['interaction_action']=='思考','E interaction prompt is visible: '+label)
  actors=cmd('actors',filter='WenSiMirrorExhibit',parts=True)['actors']
  outlines=[(a['kind'],part['name']) for a in actors for part in a['parts'] if part['outline'] and part['visible']]
  check(outlines==[(0,'ArtVisual')],'Stone outline is isolated from overlapping exhibits: '+label)
  cmd('shot',name='stone-'+label)
 c.key('E');time.sleep(.3);c.key('Q');time.sleep(.3)
 check(cmd('state')['interaction_action']=='思考','Closing thoughts restores the E prompt')
 bases=cmd('actors',filter='MuseumGoldContactBase',parts=True)['actors'];check(len(bases)==2 and all('MI_MuseumRoughGold' in a['parts'][0]['material'] for a in bases),'Both terminal bases use the gold material')
 origin=cmd('actors',filter='WenSiMirrorPuzzle')['actors'][0]['position']
 cmd('pose',eye=[origin[0]+1720,origin[1]+120,origin[2]+105],target=[origin[0]+1900,origin[1]+120,origin[2]+55]);time.sleep(1);cmd('shot',name='gold-contact-bases')
 cmd('area_fixture',area=1);time.sleep(3)
 stage=cmd('actors',filter='PopupCloseStage',parts=True)['actors'][0]
 door=next(p for p in stage['parts'] if p['name']=='OfficeBlastDoor');check(door['visible'],'First office blast door is visible before collecting the bomb')
 q=door['position'];forward=stage['forward'];eye=[q[i]-forward[i]*320+(20 if i==2 else 0) for i in range(3)]
 cmd('pose',eye=eye,target=q);time.sleep(1);cmd('shot',name='first-office-exit-bomb-stencil')
 devices=sorted(cmd('actors',filter='PopupOfficeDevice')['actors'],key=lambda a:a['name'])
 device=devices[0];p=device['position'];f=device['forward'];right=[-f[1],f[0],0]
 for n,(back,side,z) in enumerate([(190,0,170),(190,-85,170),(190,85,170),(155,0,145)]):
  eye=[p[i]-back*f[i]+side*right[i]+(z if i==2 else 0) for i in range(3)];target=[p[0],p[1],p[2]+124]
  cmd('pose',eye=eye,target=target);time.sleep(.4);cmd('shot',name=f'camera-{n}-before')
  cmd('sample_start',label=f'entry-{n}');c.key('E')
  time.sleep(.8);s=cmd('state');sample=cmd('sample_end');report['camera'].append(sample)
  check('PopupOfficeDevice' in s['view_target'],f'E enters computer from approach {n}')
  check(sample['camera_obstruction_samples']==0,f'Entry camera never intersects furniture on approach {n}')
  check(sample['max_camera_step_cm']<45,f'Entry camera remains continuous on approach {n}')
  cmd('sample_start',label=f'exit-{n}');c.key('E');time.sleep(1);sample=cmd('sample_end');report['camera'].append(sample)
  check(sample['camera_obstruction_samples']==0,f'Exit camera never intersects furniture on approach {n}')
  check(not cmd('state')['move_ignored'],f'Exit restores movement on approach {n}')
  if n in (0,3):
   # Readback stalls rendering; capture separately from the frame continuity sample.
   c.key('E')
   for k in range(4):cmd('shot',name=f'camera-{n}-entry-{k}')
   time.sleep(.4);c.key('E');time.sleep(1)
 report['passed']=True
except Exception:
 report['error']=traceback.format_exc();print(report['error'],flush=True)
finally:
 if proc and proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=45)
  except Exception:proc.terminate();proc.wait(timeout=15)
 report['exit_code']=proc.returncode if proc else None
 (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if not report['passed']:raise SystemExit(1)
