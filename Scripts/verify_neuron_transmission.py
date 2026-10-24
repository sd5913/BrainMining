"""Isolated Shipping test: actual vessel traversal and neuron jump inputs."""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('output');a=p.parse_args()
out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);profile=out/'profile';assert not profile.exists()
proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-MiningQA','-UserDir='+str(profile)])
c=Client(profile);checks=[]
def cmd(op,**kw):
    r=c.command(op,**kw);assert r['ok'],(op,kw,r);return r
def wait(pred):
    end=time.monotonic()+120
    while time.monotonic()<end:
        s=cmd('state')
        if pred(s):return s
        time.sleep(.1)
    raise AssertionError(s)
def check(value,label):
    assert value,label
    checks.append(label);print('PASS '+label,flush=True)
try:
    c.command('state',timeout=180);wait(lambda s:s['opening']);cmd('start');wait(lambda s:not s['opening'])
    cmd('gm_map',index=4);wait(lambda s:s.get('ending_gate')==0);time.sleep(2)
    gate=next(x for x in cmd('actors',filter='DiggableRock')['actors'] if '/EndingEmergence/' in x['path'])
    q=gate['position'];cmd('pose',eye=[q[0]-310,q[1],q[2]+60],target=[q[0]+400,q[1],q[2]+60]);time.sleep(1)
    cmd('key',key='W',down=True)
    try:wait(lambda s:s.get('ending_gate')==1)
    finally:cmd('key',key='W',down=False)
    wait(lambda s:s.get('ending_gate')==2);cmd('shot',name='neuron-transmission-hint')
    s=cmd('neurons');check(s['visible_blood_parts']==0 and s['visible_cell_parts']==0,'blood and cell remain hidden during neurons')
    visited={0}
    for index in (2,0,4,9,14,19,24,29,34,39,44,46):
        ns=cmd('neurons')['neurons'];target=ns[index];check(target['reachable'],f'neuron {index} reachable')
        cmd('aim',target=target['position']);wait(lambda s:s['target_neuron']==index)
        tint=cmd('neurons')['neurons'][index]['tint'];check(tint[0]>tint[2]*2,f'aimed neuron {index} still highlights gold')
        c.key('SpaceBar');wait(lambda s:s['neuron']==index and s['travelling_neuron']==-1);visited.add(index)
        if index!=46:
            pos=cmd('state')['camera'];cmd('aim',target=[pos[0],pos[1],pos[2]+2000]);time.sleep(.5)
            ns=cmd('neurons')['neurons']
            check(all(ns[i]['tint'][2]>ns[i]['tint'][0] for i in visited),f'previously visited neurons return to blue after jump {index}')
            if index==9:cmd('shot',name='visited-neurons-blue')
    time.sleep(.6);s=cmd('neurons');check(s['ending_gate']==5,'final arrival starts ascent')
    check(all(x['tint'][2]>x['tint'][0] for x in s['neurons']),'ascent retains original blue panorama')
    cmd('shot',name='neuron-ascent-blue');time.sleep(.3)
    (out/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf8')
finally:
    if proc.poll() is None:cmd('quit');proc.wait(timeout=60)
