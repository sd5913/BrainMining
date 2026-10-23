"""Shipping regression for generated terrain save identities and the cell exit.

Uses isolated profiles and explicit checkpoint positioning, not a full manual run.
"""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');args=ap.parse_args()
exe=Path(args.exe).resolve();out=Path(args.output).resolve();profile=out/'profile'
assert not profile.exists(),'Use a fresh isolated profile'
checks=[];process=None
def check(ok,label):
 assert ok,label
 checks.append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
 r=c.command(op,**kw);assert r['ok'],(op,kw);return r
def wait(pred,seconds=90):
 deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  s=c.command('state')
  if pred(s):return s
  time.sleep(.3)
 raise AssertionError('condition timeout: '+str(s))
def launch():
 global process,c
 process=subprocess.Popen([str(exe),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
 c=Client(profile);c.command('state',timeout=100);wait(lambda s:bool(s['menu_buttons']))
def close():
 if process and process.poll() is None:c.command('quit');process.wait(timeout=60)
def rock():
 s=c.command('state');path=s['computer_rocks'][1]
 return next(a for a in c.command('actors',filter='DiggableRock')['actors'] if a['path']==path)
def resume():
 cmd('continue',timeout=120);wait(lambda s:not s['opening']);time.sleep(1)
try:
 launch();cmd('start');wait(lambda s:not s['opening'])
 cmd('gm_tool',tool=0);cmd('gm_tool',tool=1);cmd('gm_tool',tool=2)
 cmd('area_fixture',area=3);a=rock();cmd('mine',actor=a['path'],point=a['center'],radius=180);time.sleep(2)
 expected=rock();check(expected['cuts']>0,'real cut made in generated television fill')
 cmd('save');cmd('return_menu');wait(lambda s:s['opening']);resume();actual=rock()
 check((actual['cuts'],actual['triangles'])==(expected['cuts'],expected['triangles']),'runtime TV terrain survives return-menu and continue')
 close();launch();resume();actual=rock()
 check((actual['cuts'],actual['triangles'])==(expected['cuts'],expected['triangles']),'runtime TV terrain survives process restart')
 check(c.command('state')['upgrade']==1 and c.command('state')['drill'],'campaign equipment survives both loads')
 cmd('gm_map',index=5);s=wait(lambda s:s.get('ending_gate')==0);time.sleep(2);s=c.command('state')
 check(s['mining_suspended'] and not s['pickaxe_visible'],'cell entry hides pickaxe and blocks excavation')
 gate=next(a for a in c.command('actors',filter='DiggableRock')['actors'] if '/EndingEmergence/' in a['path'])
 check(not gate['collision'] and gate['cuts']==0,'cell membrane is intact and has no actor collision')
 p=gate['position'];cmd('pose',eye=[p[0]-310,p[1],p[2]+60],target=[p[0]+400,p[1],p[2]+60]);time.sleep(1)
 cmd('shot',name='cell-enter-guide');c.key('LeftMouseButton');time.sleep(.8)
 gate=next(a for a in c.command('actors',filter='DiggableRock')['actors'] if a['path']==gate['path'])
 check(gate['cuts']==0,'mouse mining input cannot cut cell membrane')
 cmd('key',key='W',down=True)
 try:s=wait(lambda s:s.get('ending_gate',0)>=1,15)
 finally:cmd('key',key='W',down=False)
 check(s['ending_gate']==1,'ordinary forward input crosses membrane into vessel')
 cmd('shot',name='cell-vessel-entry');s=wait(lambda s:s.get('ending_gate')==2,90)
 check(not s['pickaxe_visible'],'pickaxe remains hidden through vessel and neurons')
 cmd('shot',name='neuron-key-hint')
 visited={0}
 for index in (2,0,4,9,14,19,24,29,34,39,44,46):
  ns=cmd('neurons')['neurons'];target=ns[index];check(target['reachable'],'neuron reachable: '+str(index))
  cmd('aim',target=target['position']);wait(lambda s:s['target_neuron']==index,10)
  c.key('SpaceBar');wait(lambda s:s['neuron']==index and s['travelling_neuron']==-1,10);visited.add(index)
  if index!=46:
   # Look away so a selected target cannot masquerade as visited gold.
   pos=c.command('state')['camera'];cmd('aim',target=[pos[0],pos[1],pos[2]+2000]);time.sleep(.4)
   ns=cmd('neurons')['neurons']
   check(all(ns[i]['tint'][0]>ns[i]['tint'][2]*2 for i in visited),'all landed neurons remain gold: '+str(index))
   if index==9:cmd('aim',target=ns[0]['position']);time.sleep(.3);cmd('shot',name='visited-neurons-gold')
 time.sleep(.6);s=cmd('neurons')
 check(s['ending_gate']==5,'last connection starts camera ascent')
 check(all(n['tint'][2]>n['tint'][0] for n in s['neurons']),'all gold visited effects restore to blue for ascent')
 cmd('shot',name='neuron-ascent-blue')
 (out/'results.json').write_text(json.dumps({'passed':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf8')
finally:close()
