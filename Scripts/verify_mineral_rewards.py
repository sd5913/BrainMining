"""Isolated integration test: 55 real pickup transactions on exposed QA ore.
Ore placement/exposure are fixtures; ownership, thresholds, saving and UI use
production paths. This is not an uninterrupted manual excavation playthrough.
"""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');ap.add_argument('--editor',action='store_true');ap.add_argument('--legacy',action='store_true');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True)
profile=out/'profile';assert not profile.exists();checks=[];p=None
def cmd(op,**kw):
    r=c.command(op,**kw);assert r['ok'],(op,kw,r);return r
def wait(pred,seconds=90):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        s=cmd('state')
        if pred(s):return s
        time.sleep(.15)
    raise AssertionError(('timeout',s))
def check(value,label):
    assert value,label
    checks.append(label);print('PASS '+label,flush=True)
def launch():
    global p,c
    args=[str(Path(a.exe).resolve())]
    if a.editor:args += [str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-noraytracing','-DisablePlugins=AllToolsets,ModelContextProtocol']
    p=subprocess.Popen(args+['-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
    c=Client(profile);c.command('state',timeout=180);return wait(lambda s:s['opening'] and s['menu_buttons'])
def close():
    if p and p.poll() is None:cmd('quit');p.wait(timeout=60)
def actors(tag):return cmd('actors',filter=tag)['actors']
try:
    launch();cmd('start');wait(lambda s:not s['opening']);time.sleep(1)
    for pickup in actors('StarterEquipment'):
        cmd('pose',actor=pickup['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
    check(cmd('state')['mineral_totals']==[0,0,0,0],'new run has zero cumulative pickups')
    for mineral in ['2','4','6','9']:cmd('mineral_fixture',mineral=mineral,amount=80)
    s=cmd('state');check(s['mineral_totals']==[0,0,0,0] and s['collection_counts'][12:]==[0]*4,'GM material grants cannot unlock rewards')
    c.key('C');time.sleep(.4)
    for _ in range(3):cmd('slate_key',key='Right')
    cmd('catalogue_rotate',item=12,yaw=0,pitch=0);cmd('shot',name='locked-reward-conditions');time.sleep(.3);c.key('C')
    target_counts={'2':25,'4':15,'6':10,'9':5};picked={m:0 for m in target_counts};milestone_shots=set()
    # Spend GM resources; later real pickups still count independently.
    cmd('upgrade');check(cmd('state')['upgrade']==1,'upgrade spend is independent of cumulative pickup progress')
    for area,tag in [(0,'BM_Campaign_MainGame01'),(2,'BM_Campaign_MainGame02'),(4,'BM_Campaign_MainGame03')]:
        cmd('area_fixture',area=area);time.sleep(2)
        s=cmd('state');entry=s['areas'][area]['entry'];point=[entry[0],entry[1],30000]
        ores=[r for r in actors(tag) if r.get('mineral') in target_counts and not r['collected']]
        for ore in ores:
            m=ore['mineral'];index=['2','4','6','9'].index(m)
            if picked[m]>=target_counts[m]:continue
            cmd('mineral_pickup_fixture',actor=ore['path'],point=point)
            cmd('pose',eye=[point[0]-110,point[1],point[2]+20],target=point);time.sleep(.25);cmd('aim',target=point);time.sleep(.15)
            cmd('treasure');picked[m]+=1;s=cmd('state')
            check(s['mineral_totals'][index]==picked[m],f'{m} successful pickup {picked[m]} counted exactly once')
            if picked[m]<target_counts[m]:check(s['collection_counts'][12+index]==0,f'{m} still locked below threshold {picked[m]}')
            else:
                check(s['collection_counts'][12+index]==1,f'{m} automatically unlocks at exact threshold')
                wait(lambda s:s.get('pickup_id')==12+index,15);cmd('shot',name='reward-reveal-'+m);time.sleep(1.25);cmd('shot',name='reward-flight-'+m)
                wait(lambda s:s.get('pickup_id')==-1,15);milestone_shots.add(m)
    check(picked==target_counts and len(milestone_shots)==4,'all four exact thresholds reached through successful ore transactions')
    check(cmd('state')['pickup_flights']==4,'four rewards each fly into the collection shortcut once')
    # All models have a visible, rotatable catalogue presentation.
    c.key('C');time.sleep(.4)
    for item in range(12,16):
        cmd('catalogue_rotate',item=item,yaw=0,pitch=0);time.sleep(3.5);cmd('shot',name='inspect-'+str(item));time.sleep(.3)
        cmd('catalogue_rotate',item=item,yaw=45,pitch=15);time.sleep(.4);cmd('shot',name='inspect-rotated-'+str(item));time.sleep(.3)
    c.key('C')
    # The existing heaven toilet must use the same bag flight.
    cmd('area_fixture',area=4);time.sleep(1)
    toilet=next(r for r in actors('MiningCollectible') if r.get('item')==0 and r['position'][2]>0)
    q=toilet['position'];cmd('pose',eye=[q[0],q[1]-160,q[2]+80],target=[q[0],q[1],q[2]+35]);time.sleep(.6);cmd('collect')
    wait(lambda s:s.get('pickup_id')==0);cmd('shot',name='existing-toilet-reveal')
    # Opening C during reveal suspends flight and keeps the award.
    c.key('C');time.sleep(.5);before=cmd('state');time.sleep(.5);after=cmd('state');check(abs(before['pickup_age']-after['pickup_age'])<.05,'catalogue pauses an in-flight collection animation')
    cmd('catalogue_rotate',item=13,yaw=0,pitch=0);c.key('C');wait(lambda s:s.get('pickup_id')==-1)
    check(cmd('state')['pickup_flights']==5,'ordinary collectible completes the same flight after closing catalogue')
    cmd('save');close();launch();cmd('menu_click',action='continue');s=wait(lambda s:not s['opening']);time.sleep(1);s=cmd('state')
    check(s['mineral_totals']==[25,15,10,5] and all(s['collection_counts'][12:]),'continue restores cumulative progress and all rewards')
    check(s['pickup_flights']==0 and s['pickup_id']==-1,'loading unlocked rewards does not replay pickup animations')
    if a.legacy:
        cmd('legacy_collection_save_fixture');close();launch();cmd('continue');wait(lambda s:not s['opening']);s=cmd('state')
        check(s['mineral_totals']==[25,15,10,5] and all(s['collection_counts'][12:]),'v4 save migration reconstructs all four totals from actual collected placement IDs')
        wait(lambda s:s['pickup_flights']==4 and s['pickup_id']==-1,30)
        check(True,'newly eligible legacy-save rewards each receive one collection flight')
    cmd('return_menu');wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:s['collection_choice']);cmd('confirm_new',keep=True);s=wait(lambda s:not s['opening']);time.sleep(.5);s=cmd('state')
    check(s['mineral_totals']==[0]*4 and all(s['collection_counts'][12:]) and s['upgrade']==0,'Keep preserves unlocked rewards while resetting all new-run mineral totals and upgrades')
    check(s['pickup_id']==-1,'Keep does not replay existing rewards')
    toilet=next(r for r in actors('MiningCollectible') if r.get('item')==0 and r['position'][2]>0);check(not toilet['collected'],'Keep respawns world collectibles')
    cmd('save');cmd('return_menu');wait(lambda s:s['opening'] and s['menu_buttons']);cmd('start');wait(lambda s:s['collection_choice']);cmd('confirm_new',keep=False);s=wait(lambda s:not s['opening']);time.sleep(.5);s=cmd('state')
    check(s['mineral_totals']==[0]*4 and s['collection']==0,'Clear removes all catalogue unlocks and cumulative progress')
    (out/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf8')
finally:
    close()
    if not (out/'results.json').exists():(out/'partial.json').write_text(json.dumps(checks,ensure_ascii=False),encoding='utf8')
