"""Click every locked UMG card, then capture current models and offline viewport animation."""
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
    end=time.monotonic()+180
    while time.monotonic()<end:
        s=cmd('state')
        if pred(s):return s
        time.sleep(.1)
    raise AssertionError(s)
def check(value,label):
    assert value,label
    checks.append(label);print('PASS '+label,flush=True)
try:
    c.command('state',timeout=180);wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:not s['opening'])
    for item in cmd('actors',filter='StarterEquipment')['actors']:
        cmd('pose',actor=item['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
    c.key('C');time.sleep(.7)
    check(cmd('state')['catalogue_count']=='已收集0/16','new catalogue shows 0 of 16')
    for page in range(4):
        if page:cmd('catalogue_button',name='NextButton')
        for slot in range(4):
            item=page*4+slot
            cmd('catalogue_button',name='ItemButton'+str(slot));s=cmd('state')
            check(s['selected_item']==item and '尚未解锁' in s['catalogue_description'] and not s['catalogue_preview_visible'],f'locked card {item} selectable with locked text and hidden model')
            if item>=12:
                required=['25个铜矿','15个铁矿','10个孔雀石','5个钻石'][item-12]
                check(required in s['catalogue_description'] and '当前进度：0 /' in s['catalogue_description'],f'locked reward {item} shows threshold and progress')
            if item in [0,12,13,14,15]:
                time.sleep(.2);cmd('shot',name='locked-'+str(item));time.sleep(.3)
    c.key('C');cmd('area_fixture',area=0);time.sleep(1)
    cmd('gm_menu');cmd('slate_focus',tag='GM_UnlockMineral_12');time.sleep(.3);cmd('shot',name='gm-unlock-menu');time.sleep(.3);cmd('gm_menu')
    cmd('gm_mineral',item=-1)
    s=wait(lambda s:s['pickup_flights']==4 and s['pickup_id']==-1);check(True,'four rewards complete four pickup flights')
    c.key('C');time.sleep(.5)
    check(cmd('state')['catalogue_count']=='已收集4/16','mineral unlocks update catalogue count to 4 of 16')
    for item in range(12,16):
        cmd('catalogue_rotate',item=item,yaw=0,pitch=0);time.sleep(3.5);cmd('shot',name='inspect-'+str(item));time.sleep(.3)
    c.key('C');cmd('area_fixture',area=4);time.sleep(1)
    toilet=next(x for x in cmd('actors',filter='MiningCollectible')['actors'] if x.get('item')==0 and x['position'][2]>0)
    q=toilet['position'];cmd('pose',eye=[q[0],q[1]-160,q[2]+80],target=[q[0],q[1],q[2]+35]);time.sleep(.6);cmd('collect')
    wait(lambda s:s['pickup_flights']==5 and s['pickup_id']==-1)
    check(True,'ordinary collectible also completes flight to shortcut')
    c.key('C');time.sleep(.5);check(cmd('state')['catalogue_count']=='已收集5/16','ordinary pickup updates unique count to 5 of 16')
    cmd('shot',name='count-after-ordinary');time.sleep(.3)
    (out/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf8')
finally:
    if proc.poll() is None:cmd('quit');proc.wait(timeout=60)
