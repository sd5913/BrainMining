"""Frame-time samples in real Shipping rooms, using explicit checkpoint placement."""
import argparse,json,time,subprocess
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');args=ap.parse_args()
out=Path(args.output).resolve();profile=out/'profile';assert not profile.exists()
p=subprocess.Popen([str(Path(args.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
c=Client(profile);samples=[]
def cmd(op,**kw):
 r=c.command(op,**kw);assert r['ok'],r;return r
def sample(label,seconds):
 r=c.sample(label,seconds);samples.append(r);print(label+' '+json.dumps(r['metrics']),flush=True)
try:
 c.command('state',timeout=180)
 while not c.command('state')['menu_buttons']:time.sleep(.2)
 cmd('start')
 while c.command('state')['opening']:time.sleep(.2)
 cmd('gm_tool',tool=0);cmd('area_fixture',area=3);time.sleep(3)
 stage=cmd('actors',filter='CardRoomStage')['actors'][0];q=stage['position']
 cmd('pose',eye=[q[0]-75,q[1],q[2]+115],target=q);time.sleep(2)
 cmd('card',action='interact');time.sleep(.5);cmd('card',action='interact');time.sleep(5)
 for round in range(1,5):
  s=c.command('state');assert s['card_round']==round and s['card_phase']==4
  cmd('card',action='all');cmd('card',action='confirm');sample('card-round-'+str(round),5.2)
 cmd('card',action='interact');time.sleep(5)
 s=c.command('state');checkpoint=s['tv_checkpoint_position'];cmd('pose',eye=[checkpoint[0],checkpoint[1],checkpoint[2]+64],target=s['tv_entry']);time.sleep(6)
 assert c.command('state')['tv_checkpoint']
 rocks=cmd('actors',filter='DiggableRock')['actors'];screen=next(r for r in rocks if r['path']==s['computer_rocks'][0]);cmd('mine',actor=screen['path'],point=screen['center'],radius=500);time.sleep(2)
 q=s['tv_entry'];cmd('pose',eye=[q[0]+250,q[1],q[2]+197],target=[q[0]+750,q[1],q[2]+197]);time.sleep(4)
 assert c.command('state')['inside_computer'],'television interior checkpoint'
 sample('television-interior',30)
 cmd('area_fixture',area=1);time.sleep(3)
 d=cmd('actors',filter='PopupOfficeDevice')['actors'][0];q=d['position'];f=d['forward']
 cmd('pose',eye=[q[i]-f[i]*215+(158 if i==2 else 0) for i in range(3)],target=[q[i]+(124 if i==2 else 0) for i in range(3)]);time.sleep(3)
 cmd('office');sample('office-running',20);c.key('E');time.sleep(1);sample('office-screen',15);c.key('E');time.sleep(1)
 deadline=time.monotonic()+100
 while 'phase=1 ' in c.command('state')['office']:
  assert time.monotonic()<deadline,'office round did not advance';time.sleep(1)
 sample('office-eruption',12)
 (out/'results.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2),encoding='utf8')
finally:
 if p.poll() is None:c.command('quit');p.wait(timeout=60)
