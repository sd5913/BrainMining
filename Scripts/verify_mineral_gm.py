"""Verify real GM buttons, persistence and record the actual game viewport.
All changes are confined to an explicit, new QA profile. No desktop capture.
"""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
p=argparse.ArgumentParser();p.add_argument('exe');p.add_argument('output');p.add_argument('--manual-start',action='store_true');a=p.parse_args()
out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);profile=out/'profile';assert not profile.exists()
checks=[];proc=None;c=Client(profile)
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
def launch():
    global proc
    proc=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-MiningQA','-UserDir='+str(profile)])
    c.command('state',timeout=180);wait(lambda s:s['opening'] and s['menu_buttons'])
def close():
    if proc and proc.poll() is None:cmd('quit');proc.wait(timeout=60)
def equipment():
    for item in cmd('actors',filter='StarterEquipment')['actors']:
        cmd('pose',actor=item['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
def click(item):
    cmd('gm_menu');time.sleep(.3)
    cmd('slate_focus',tag='GM_UnlockMineral_'+str(item));time.sleep(.3)
    if item==12:cmd('shot',name='gm-unlock-menu');time.sleep(.2)
    cmd('slate_activate',tag='GM_UnlockMineral_'+str(item))
try:
    launch();cmd('start');wait(lambda s:not s['opening']);equipment()
    if a.manual_start:
        print('READY: waiting for native GM check and resume.txt',flush=True)
        deadline=time.monotonic()+300
        while not (out/'resume.txt').exists():
            if time.monotonic()>deadline:raise TimeoutError('native check')
            time.sleep(.2)
    paths=cmd('state')['gm_maps']
    check(len(paths)==5 and not any('Lvl_CardRoom_P1' in x or 'FullJourney' in x for x in paths),'two unwanted standalone GM entries removed')
    for key in ['F4','F6','F7','F9']:cmd('slate_key',key=key,ctrl=key=='F9')
    s=cmd('state');check(not s['ui_hidden'] and not s['menu_open'] and s['area']==0,'removed debug shortcuts do not hide UI, jump or open legacy menus')
    cmd('slate_key',key='F10',ctrl=True);s=cmd('state')
    check(s['menu_open'] and s['upgrade']==0 and not s['bomb'] and not s['drill'],'Ctrl F10 only opens GM and grants no equipment')
    cmd('slate_activate',tag='GM_ToggleUI');check(cmd('state')['ui_hidden'],'GM button hides UI')
    cmd('gm_menu');time.sleep(.2);cmd('shot',name='gm-while-ui-hidden');cmd('slate_activate',tag='GM_ToggleUI')
    check(not cmd('state')['ui_hidden'],'GM stays accessible and its button restores UI')
    cmd('gm_menu');time.sleep(.3);cmd('shot',name='gm-route-menu');cmd('gm_menu')
    for area in [0,1,2,3,4,5,0]:
        cmd('gm_jump',area=area);s=wait(lambda s:s['area']==area and not s['menu_open'])
        check(not s['move_ignored'] and not s['look_ignored'],f'GM route {area} reaches campaign entrance and restores controls')
    time.sleep(1);cmd('save');cmd('gm_map',index=4);wait(lambda s:s['map']=='Lvl_EndingEmergence_Demo');time.sleep(1)
    cmd('gm_jump',area=2);s=wait(lambda s:s['map']=='Lvl_MainGame' and not s['opening'] and s['area']==2)
    check(not s['menu_open'],'standalone ending can jump back to campaign passage 2')
    cmd('area_fixture',area=4);time.sleep(1)
    toilet=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==0 and x['position'][2]>0)
    q=toilet['position'];cmd('pose',eye=[q[0],q[1]-160,q[2]+80],target=[q[0],q[1],q[2]+35]);time.sleep(.6);cmd('collect')
    wait(lambda s:s['pickup_flights']==1 and s['pickup_id']==-1)
    original=cmd('state')['collection_counts'][:12]
    cmd('area_fixture',area=0);time.sleep(1)
    for item in range(12,16):
        click(item);s=wait(lambda s:s['pickup_flights']==item-10 and s['pickup_id']==-1)
        check(s['collection_counts'][item]==1 and s['collection_counts'][:12]==original,f'GM button {item} grants once and preserves existing collectibles')
        check(s['mineral_totals']==[0]*4,f'GM button {item} does not change mineral pickup progress')
    before=cmd('state');cmd('gm_mineral',item=-1);time.sleep(.5);after=cmd('state')
    check(after['collection_counts']==before['collection_counts'] and after['pickup_flights']==before['pickup_flights'],'repeat unlock-all neither duplicates rewards nor replays flights')
    cmd('save');close();launch();cmd('continue');wait(lambda s:not s['opening']);s=cmd('state')
    check(s['collection_counts'][:12]==original and s['collection_counts'][12:]==[1]*4,'GM unlocks and old collectibles survive process restart')
    check(s['mineral_totals']==[0]*4 and s['pickup_id']==-1,'continue preserves real pickup counts and does not replay GM rewards')
    cmd('return_menu');wait(lambda s:s['opening']);cmd('start');wait(lambda s:s['collection_choice']);cmd('confirm_new',keep=False);wait(lambda s:not s['opening']);equipment()
    cmd('area_fixture',area=0);time.sleep(1)
    cmd('gm_mineral',item=-1)
    s=wait(lambda s:s['pickup_flights']==4 and s['pickup_id']==-1)
    check(s['collection_counts'][12:]==[1]*4 and s['mineral_totals']==[0]*4,'unlock-all grants four rewards with four consecutive flights')
    c.key('C');time.sleep(.5)
    for item in range(12,16):
        cmd('catalogue_rotate',item=item,yaw=0,pitch=0);time.sleep(3.5);cmd('shot',name='inspect-'+str(item));time.sleep(.2)
    (out/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf8')
finally:
    close()
