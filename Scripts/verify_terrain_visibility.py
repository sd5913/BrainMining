"""Isolated regression for terrain visibility after room transfers and E pickups."""
import argparse, json, subprocess, time, traceback
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True)
profile=out/'profile';profile.mkdir()
config=profile/'Saved/Config/Windows';config.mkdir(parents=True)
(config/'Engine.ini').write_text('[Audio]\nUnfocusedVolumeMultiplier=1.0\n')
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
proc=subprocess.Popen(launch+['-RenderOffscreen','-windowed','-ResX=1280','-ResY=870','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'checks':[],'passed':False}
def cmd(op,**kw):
    s=c.command(op,timeout=180,**kw);assert s['ok'],(op,kw);return s
def check(ok,label):
    r['checks'].append(dict(passed=bool(ok),name=label));assert ok,label
    print('PASS',label,flush=True)
def actors(name,parts=False):return cmd('actors',filter=name,parts=parts)['actors']
def plus(a,b):return [x+y for x,y in zip(a,b)]
def shot(name):cmd('shot',name=name);time.sleep(.5)
try:
    cmd('state');cmd('start')
    for _ in range(120):
        if not cmd('state')['opening']:break
        time.sleep(.2)
    def terrain(label):
        time.sleep(1.5)
        state=cmd('state');rocks=actors('GeneratedMiningRock')
        missing=[x for x in rocks if x.get('near_missing_chunks',0)>0]
        r.setdefault('snapshots',[]).append({'label':label,'area':state['area'],'camera':state['camera'],'missing':missing})
        check(not missing,label+' has no missing nearby solid terrain')
    cmd('gm_jump',area=2);time.sleep(4)
    initial=cmd('state');eye=initial['camera'];target=plus(eye,[200,0,0]);r['entry']=initial
    r['before']=actors('GeneratedMiningRock')
    cmd('pose',eye=plus(eye,[0,12000,0]),target=plus(eye,[200,12000,0]));time.sleep(1)
    cmd('gm_jump',area=3);time.sleep(1)
    cmd('pose',eye=eye,target=target);time.sleep(2)
    r['after']=actors('GeneratedMiningRock')
    r['missing']=[x for x in r['after'] if x.get('near_missing_chunks',0)>0]
    check(not r['missing'],'Previously hidden second-passage terrain recovers after entering the room')
    eye=cmd('state')['camera'];cmd('aim',target=plus(eye,[-2500,0,0]));shot('room-look-back')
    # Revisit all three passage/room boundaries, including already initialized chunks.
    for area in [0,1,2,3,4,5,2]:
        cmd('gm_jump',area=area);terrain('Area '+str(area))
    # Actual E input on existing ore actors, with exposure/progress setup isolated
    # from the player's save. Exercises reward-preview creation and actor registration.
    eye=cmd('state')['camera'];ores=actors('MiningTreasure')
    r['ore_candidates']=[x for x in ores if x['class']=='MiningTreasure' and -14500<x['position'][0]<-500]
    for mineral,threshold in [('2',25),('4',15),('6',10),('9',5)]:
        options=[x for x in r['ore_candidates'] if x.get('mineral')==mineral and not x['collected']]
        if not options:continue
        ore=options[0];point=plus(eye,[85,0,0])
        cmd('mineral_progress_fixture',mineral=int(mineral),amount=threshold-1)
        cmd('mineral_pickup_fixture',actor=ore['path'],point=point)
        cmd('aim',target=point);time.sleep(.2)
        cmd('sample_start',label='ore-'+mineral);c.key('E');time.sleep(2)
        sample=cmd('sample_end');r.setdefault('pickup_frames',{})[mineral]=sample['frame_ms']
        actual=next(x for x in actors('MiningTreasure') if x['path']==ore['path'])
        check(actual['collected'],'E collects mineral '+mineral+' at its inspiration threshold')
        terrain('After E pickup '+mineral)
    assert {'2','4','6'}.issubset(r.get('pickup_frames',{})), 'Second-passage pickup coverage is incomplete'
    cmd('save');terrain('After save snapshot')
    r['passed']=True
except:
    r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
    (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    try:cmd('quit');proc.wait(timeout=30)
    except:proc.terminate()
if not r['passed']:raise SystemExit(1)
