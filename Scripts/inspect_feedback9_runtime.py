import subprocess,time,json
from pathlib import Path
from packaged_qa_client import Client
root=Path(__file__).resolve().parents[1];out=root/'Saved/Validation/Feedback9/Before';out.mkdir(parents=True,exist_ok=True)
profile=out/'profile';assert not profile.exists()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
exe=root/'Saved/Validation/PCSize/PCSize-20261025-AudioSceneFix/Optimized/Archive/Windows/WOoOW.exe'
p=subprocess.Popen([str(exe),'-MiningQA','-windowed','-ResX=1280','-ResY=720','-UserDir='+str(profile)],startupinfo=si);c=Client(profile);r={}
def cmd(op,**kw):
 s=c.command(op,timeout=180,**kw);assert s['ok'],s;return s
def shot(name,eye,target):cmd('pose',eye=eye,target=target);time.sleep(.7);cmd('shot',name=name);time.sleep(.5)
try:
 cmd('state');cmd('start');time.sleep(3);cmd('gm_tool',tool=0)
 shot('copper-toward-7',[-27820,820,10700],[-28358,554,10967])
 shot('copper-toward-9',[-27930,800,10700],[-27072,928,10688])
 r['ground']=[]
 for x,y,z in [(-27930,800,10700),(-27200,800,10700),(-28300,800,11000)]:r['ground'].append(cmd('trace',**{'from':[x,y,z],'to':[x,y,z-900]}))
 cmd('area_fixture',area=4);time.sleep(2)
 shot('bomb-36',[39060,10829,16956],[39486,10829,16956])
 shot('bomb-38',[45000,12145,15241],[45380,12145,15241])
 shot('bomb-41',[38880,12980,17368],[38880,12560,17368])
 r['actors']=cmd('actors',filter='MiningCollectible')['actors']
finally:
 (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');cmd('quit');p.wait(timeout=30)
