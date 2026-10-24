"""Isolated native preview, catalogue ordering, and mineral unlock subtitles."""
import argparse,json,subprocess,time,traceback,re,sys
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('output');p.add_argument('--exe');p.add_argument('--rhi',choices=['d3d11','d3d12']);p.add_argument('--office-only',action='store_true');p.add_argument('--disable-raytracing',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists();out.mkdir(parents=True)
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
if a.rhi:launch.append('-'+a.rhi)
if a.disable_raytracing:launch.append('-noraytracing')
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
proc=subprocess.Popen(launch+['-windowed','-ResX=1280','-ResY=720','-MiningQA','-UserDir='+str(profile)],startupinfo=startup)
c=Client(profile);report={'passed':False,'checks':[]}
def cmd(op,**kw):
 r=c.command(op,timeout=180,**kw);assert r['ok'],(op,kw);return r
def wait(pred,seconds=180):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=cmd('state')
  if pred(s):return s
  time.sleep(.15)
 raise AssertionError(s)
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def shot(name):cmd('shot',name=name);time.sleep(.4)
try:
 wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening']);cmd('gm_jump',area=1);time.sleep(2)
 cmd('pose',eye=[-16300,100,6560],target=[-15500,100,6750]);time.sleep(8);shot('office-ceiling')
 for name,eye,target in [('office-entry-wall',[-16500,400,6550],[-16740,620,6550]),('office-exit-wall',[-15000,-350,6550],[-14750,-500,6550])]:
  cmd('pose',eye=eye,target=target);time.sleep(2);shot(name)
 report['render_request']=dict(rhi=a.rhi,disable_raytracing=a.disable_raytracing)
 # Shipping deliberately compiles out logging; inspect its rendered captures.
 if not a.exe:
  log=(profile/'Saved/Logs/BrainMining.log').read_text(encoding='utf8',errors='replace')
  matches=re.findall(r'Office ceiling lighting: fallback_tiles=(\d+) feature_level=(\d+) ray_tracing=(\d+) missing_indirect=(\d+)',log)
  assert matches,'Missing runtime office lighting diagnostic'
  tiles,feature,rt,missing=map(int,matches[-1]);report['office_lighting']=dict(tiles=tiles,feature=feature,ray_tracing=rt,missing_indirect=missing)
  check(tiles==(35 if missing else 0),'Only the 35 office ceiling tiles use fallback when required')
  if a.rhi=='d3d11' or a.disable_raytracing:check(rt==0 and missing==1,'Compatibility mode detected without hardware tracing')
 if a.office_only:report['passed']=True;report['mode']='office visual capture only';sys.exit(0)
 cmd('gm_jump',area=0);time.sleep(1);wait(lambda s:not s['subtitle'],90)
 for Id,text in [(12,'收集到了25个铜矿！'),(13,'收集到了15个铁矿！'),(14,'收集到了10个孔雀石！'),(15,'收集到了5个钻石！')]:
  cmd('gm_mineral',item=Id);s=wait(lambda s:s['subtitle']==text,45);shot('mineral-'+str(Id))
  check(s['collection_counts'][Id]==1,'Mineral reward and subtitle '+str(Id));wait(lambda s:s['pickup_id']==-1 and not s['subtitle'],90)
  cmd('gm_mineral',item=Id);time.sleep(.4);check(not cmd('state')['subtitle'],'Already owned reward does not repeat subtitle '+str(Id))
 c.key('C');time.sleep(.5)
 for Id in range(23):cmd('catalogue_owned_fixture',item=Id)
 # Reopen at the first presentation item; the widget resets its page from that ID.
 cmd('catalogue_rotate',item=12,yaw=0,pitch=0);c.key('C');c.key('C');time.sleep(.4)
 source=(root/'Source/BrainMining/MiningCollectible.cpp').read_text(encoding='utf8');names=re.findall(r'TEXT\("([^"\\]*)"\)',source.split('const TCHAR* Names[]=')[1].split(';')[0])
 order=[12,13,16,2,1,17,4,5,14,15,18,19,3,9,6,7,11,20,0,8,21,10,22]
 for page in range(6):
  s=cmd('state');labels={x['name']:x['text'] for x in s['catalogue_labels']};expected=[names[i] for i in order[page*4:page*4+4]]
  check([labels['ItemLabel'+str(i)] for i in range(len(expected))]==expected,'Actual catalogue page '+str(page+1)+' follows journey order');shot('page-'+str(page+1))
  if page<5:cmd('catalogue_button',name='NextButton')
 for Id in [3,10,11,14,16,17,18,19,20,21,22]:
  cmd('catalogue_rotate',item=Id,yaw=0,pitch=0);time.sleep(.7);s=cmd('state');report.setdefault('descriptions',{})[str(Id)]=s['catalogue_description'];shot('inspect-'+str(Id))
 c.key('C');cmd('save');wait(lambda s:not s['save_in_flight']);report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 if proc.poll() is None:
  try:cmd('quit');proc.wait(timeout=60)
  except Exception:pass
