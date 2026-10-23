"""Short visual QA using explicitly seeded milestones and an isolated profile."""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('output');p.add_argument('--exe');a=p.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists()
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-noraytracing','-DisablePlugins=AllToolsets,ModelContextProtocol']
proc=subprocess.Popen(launch+['-windowed','-ResX=1600','-ResY=900','-MiningQA','-UserDir='+str(profile)])
c=Client(profile)
def cmd(op,**kw):
    r=c.command(op,**kw);assert r['ok'],(op,kw);return r
def wait(pred):
    end=time.monotonic()+120
    while time.monotonic()<end:
        s=cmd('state')
        if pred(s):return s
        time.sleep(.1)
    raise AssertionError(s)
try:
    c.command('state',timeout=180);wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening'])
    for pickup in cmd('actors',filter='StarterEquipment')['actors']:
        cmd('pose',actor=pickup['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
    c.key('C');time.sleep(.5)
    for _ in range(3):cmd('slate_key',key='Right')
    cmd('catalogue_rotate',item=12,yaw=0,pitch=0);time.sleep(.3);cmd('shot',name='locked-conditions-clean')
    if a.exe:assert '挖掘就能找到灵感' not in cmd('state')['subtitle']
    c.key('C')
    cmd('area_fixture',area=0);time.sleep(1)
    for m,n in [(2,25),(4,15),(6,10),(9,5)]:cmd('mineral_progress_fixture',mineral=m,amount=n)
    cmd('reward_check_fixture')
    diagnostics=[]
    for item in range(12,16):
        wait(lambda s:s['pickup_id']==item);time.sleep(.35);cmd('shot',name='reveal-'+str(item));diagnostics.append(cmd('pickup_debug'));assert diagnostics[-1]['pickup_visible_pixels']>100;time.sleep(.6);cmd('shot',name='flight-'+str(item))
    wait(lambda s:s['pickup_id']==-1)
    c.key('C');time.sleep(.5)
    for item in range(12,16):
        cmd('catalogue_rotate',item=item,yaw=0,pitch=0);time.sleep(3.5);cmd('shot',name='inspect-'+str(item));time.sleep(.25)
    (out/'diagnostics.json').write_text(json.dumps(diagnostics,ensure_ascii=False,indent=2),encoding='utf8')
finally:
    if proc.poll() is None:cmd('quit');proc.wait(timeout=60)
