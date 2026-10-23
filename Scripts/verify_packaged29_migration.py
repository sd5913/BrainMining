"""Legacy save migration and wall decal review in an isolated Shipping profile."""
import argparse,json,shutil,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');ap.add_argument('save_directory');a=ap.parse_args()
out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists()
shutil.copytree(a.save_directory,profile/'Saved/SaveGames')
p=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
c=Client(profile)
try:
 s=c.command('state',timeout=100)
 while not s.get('menu_buttons'):time.sleep(.3);s=c.command('state')
 c.command('menu_pointer',action='away',phase='move');time.sleep(1);c.command('shot',name='menu-new-perspective');time.sleep(.3)
 s=c.command('continue',timeout=120);time.sleep(2);s=c.command('state')
 assert not s['opening'] and s['upgrade']==1 and s['drill'],s
 print('PASS legacy v3 save resumes with equipment',flush=True)
 (out/'results.json').write_text(json.dumps({'passed':True,'check':'legacy v3 generated-TV save resumes with equipment','state':s},ensure_ascii=False,indent=2),encoding='utf8')
 c.command('area_fixture',area=2);time.sleep(1)
 walls=c.command('actors',filter='BM_Feedback_ThinBlastLayer')['actors'];assert walls
 wall=walls[0];center=wall['center'];extent=wall['extent']
 c.command('pose',eye=[center[0]-extent[0]-250,center[1],center[2]],target=center);time.sleep(2)
 c.command('shot',name='bomb-wall-worn');time.sleep(.5)
 (out/'wall.json').write_text(json.dumps(wall,indent=2),encoding='utf8')
finally:
 if p.poll() is None:c.command('quit');p.wait(timeout=60)
