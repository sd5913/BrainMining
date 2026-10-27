"""Isolated runtime fixtures for upgrade layout and room-to-mine audio continuity."""
import argparse, json, subprocess, time, traceback, wave, array, math
from pathlib import Path
from packaged_qa_client import Client

ap = argparse.ArgumentParser()
ap.add_argument('output')
ap.add_argument('--exe', help='Verify a packaged Windows executable instead of the editor')
args = ap.parse_args()
root = Path(__file__).resolve().parents[1]
out = Path(args.output).resolve()
out.mkdir(parents=True, exist_ok=True)
profile = out / 'profile'
assert not profile.exists(), 'Use a new isolated output directory'
profile.mkdir()
si = subprocess.STARTUPINFO()
si.dwFlags |= subprocess.STARTF_USESHOWWINDOW
si.wShowWindow = 0
launch = [str(Path(args.exe).resolve())] if args.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',
    str(root/'BrainMining.uproject'), '/Game/MainGame/Maps/Lvl_MainGame', '-game',
    '-NoLiveCoding', '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol']
proc = subprocess.Popen(launch+[
    '-windowed', '-ResX=1280', '-ResY=720', '-MiningQA', '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1', '-UserDir='+str(profile),
    '-abslog='+str(out/'game.log')], startupinfo=si)
c = Client(profile)
result = {'passed':False, 'checks':[], 'states':{}, 'recordings':{}}
def cmd(op, **kw):
    r = c.command(op, timeout=180, **kw)
    assert r['ok'], (op, kw, r)
    return r
def check(value, label):
    result['checks'].append({'passed':bool(value),'name':label})
    assert value, label
    print('PASS', label, flush=True)
def state(label):
    s = cmd('state'); result['states'][label] = s; return s
def music(s):
    return next(x for x in s['music'] if x['name']=='M_DreamyFlashback_Loop')
def capture(label):
    cmd('audio_capture_start'); time.sleep(4)
    cmd('audio_capture_stop', name=label)
    path = c.directory/(label+'.wav')
    end = time.monotonic()+15
    while not path.exists() and time.monotonic()<end: time.sleep(.2)
    with wave.open(str(path),'rb') as w:
        assert w.getsampwidth()==2
        samples = array.array('h',w.readframes(w.getnframes()))
        rms = math.sqrt(sum(float(v)*v for v in samples)/len(samples))/32768
    result['recordings'][label] = {'path':str(path),'rms':rms}
    check(rms>.0001,label+' produces non-silent mixer output')
try:
    cmd('state'); cmd('start'); time.sleep(5)
    starter = cmd('actors',filter='StarterEquipment')['actors'][0]
    cmd('pose',actor=starter['name'],offset=[-125,0,50]);cmd('interact');time.sleep(1)
    cmd('mineral_fixture',mineral='2',amount=2)
    cmd('mineral_fixture',mineral='4',amount=2)
    cmd('area_fixture',area=0);time.sleep(6)
    s=state('normal-hud'); rows=[x for x in s['upgrade_requirements'] if x['visible']]
    check(rows[0]['count']=='还需2个','Remaining materials use number and 个')
    check(rows[1]['count']=='✓ 已足够' and rows[1]['color'][1]>.9 and rows[1]['color'][0]<.5,'Enough materials show green name and checked label')
    cmd('shot',name='hud-normal');time.sleep(.5)
    first=music(s);check(first['playing'] and first['gain']>.06,'Opening-room exploration music is active at its reduced gain')
    capture('mine-before')
    for area,label in [(1,'office'),(2,'after-office'),(5,'museum'),(4,'after-museum')]:
        cmd('area_fixture',area=area);time.sleep(6)
        s=state(label);m=music(s)
        check(m['player_id']==first['player_id'] and m['playing'],label+' retains the exploration playback timeline')
        if area in [1,5]:check(m['gain']==0,label+' mutes exploration underneath the room')
        else:
            check(m['gain']>.1,label+' restores exploration gain')
            capture(label)
        if area==1:check(not any(x['name']=='M_Room_Office_Loop' and x['playing'] for x in s['music']),'Office has no newly added music')
    for mineral in ['2','4','6','9']:cmd('mineral_fixture',mineral=mineral,amount=200)
    while cmd('state')['upgrade']<4:cmd('upgrade')
    time.sleep(1);s=state('max-hud')
    check(s['upgrade_panel_size']==[128,158] and s['upgrade_hint']=='已满级','Max level collapses the panel and retains 已满级')
    check(not any(x['visible'] for x in s['upgrade_requirements']),'Max level hides requirement rows')
    cmd('shot',name='hud-max');time.sleep(.5)
    result['passed']=True
except Exception:
    result['error']=traceback.format_exc();print(result['error'],flush=True)
finally:
    (out/'results.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    try:cmd('quit');proc.wait(timeout=30)
    except Exception:proc.terminate();proc.wait(timeout=20)
if not result['passed']:raise SystemExit(1)
