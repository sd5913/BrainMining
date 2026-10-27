import subprocess,time,json
from pathlib import Path
from packaged_qa_client import Client
root=Path(__file__).resolve().parents[1];out=root/'Saved/Validation/Feedback8/Before';out.mkdir(parents=True,exist_ok=True)
profile=out/'profile';assert not profile.exists()
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
exe=root/'Saved/Validation/PCSize/PCSize-20261025-HUDMusic/Optimized/Archive/Windows/WOoOW.exe'
p=subprocess.Popen([str(exe),'-MiningQA','-windowed','-ResX=1280','-ResY=720','-UserDir='+str(profile)],startupinfo=si);c=Client(profile);r={}
try:
 c.command('state',timeout=180);c.command('start');time.sleep(3);c.command('gm_tool',tool=0)
 c.command('pose',eye=[-30450,0,12100],target=[-30100,0,11860]);time.sleep(1);c.command('shot',name='floor-before')
 soil=next(a for a in c.command('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
 c.command('mine',actor=soil['path'],point=soil['position'],radius=230);time.sleep(2)
 c.command('shot',name='floor-after-cut');r['floor']=[]
 for x in [-30300,-30200,-30150,-30100,-30000]:
  for y in [-150,0,150]:r['floor'].append(c.command('trace',**{'from':[x,y,11980],'to':[x,y,11600]}))
 for n,eye,target in [('copper-before',[-28000,700,10800],[-28344,509,11032]),('reagent-before',[-27714,955,10712],[-27823,1134,10616])]:
  c.command('pose',eye=eye,target=target);time.sleep(1);c.command('shot',name=n)
 r['actors']=c.command('actors',filter='MiningTreasure')['actors']
finally:
 (out/'inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
 c.command('quit');p.wait(timeout=30)
