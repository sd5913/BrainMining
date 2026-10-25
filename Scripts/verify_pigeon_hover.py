"""Excavate the resident pigeon, capture its feather motion, and collect normally."""
import argparse, json, math, subprocess, time, traceback
from pathlib import Path
from packaged_qa_client import Client

p = argparse.ArgumentParser()
p.add_argument('output')
p.add_argument('--exe')
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = Path(a.output).resolve()
profile = out / 'profile'
assert not profile.exists()
out.mkdir(parents=True, exist_ok=True)
launch = [str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe', str(root/'BrainMining.uproject'), '/Game/MainGame/Maps/Lvl_MainGame', '-game', '-NoLiveCoding', '-EnablePlugins=GameFeatures', '-DisablePlugins=AllToolsets,ModelContextProtocol']
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
proc = subprocess.Popen(launch + ['-windowed', '-ResX=1280', '-ResY=720', '-MiningQA', '-UserDir='+str(profile)], startupinfo=startup)
c = Client(profile)
report = dict(passed=False, checks=[])

def cmd(op, **kw):
    result = c.command(op, timeout=180, **kw)
    assert result['ok'], (op, kw)
    return result

def wait(predicate, seconds=180):
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        state = cmd('state')
        if predicate(state): return state
        time.sleep(.2)
    raise AssertionError(state)

def bird():
    return next(x for x in cmd('actors', filter='MiningCollectible')['actors'] if x.get('item') == 3 and x['position'][2] > 0)

try:
    wait(lambda s: s['opening'] and s['menu_buttons'])
    cmd('start'); wait(lambda s: not s['opening']); cmd('gm_jump', area=2)
    spec = next(x for x in json.loads((root/'Saved/Validation/EmptyDeadEnds/saved-map.json').read_text(encoding='utf8'))['collectibles'] if x['id'] == 3)
    normal = [spec['clue'][i]-spec['position'][i] for i in range(3)]
    length = math.sqrt(sum(x*x for x in normal)); normal = [x/length for x in normal]
    cmd('pose', eye=[spec['approach'][0], spec['approach'][1], spec['approach'][2]+55], target=spec['clue'])
    time.sleep(1)
    assert not bird()['released']
    report['checks'].append('Intact soil keeps pigeon locked')
    for depth in [0,90,180]:
        cmd('mine', actor=bird()['host'], point=[spec['clue'][i]-normal[i]*depth for i in range(3)], radius=195)
        time.sleep(.3)
    time.sleep(1)
    obj = bird(); assert obj['released']
    pos = obj['position']
    cmd('pose', eye=[pos[i]+normal[i]*180+(25 if i==2 else 0) for i in range(3)], target=pos)
    time.sleep(1)
    for i in range(4):
        cmd('shot', name='pigeon-hover-'+str(i)); time.sleep(.13)
    assert bird()['position'] == pos
    report['checks'].append('Flying presentation keeps pickup actor position stable')
    cmd('collect'); wait(lambda s: s['pickup_id']==-1 and s['collection_counts'][3]==1)
    report['checks'].append('Normal aim/E collection and flight to inventory complete')
    c.key('C'); cmd('catalogue_rotate', item=3, yaw=0, pitch=0); time.sleep(1)
    for i in range(3):
        cmd('shot', name='pigeon-inspect-'+str(i)); time.sleep(.13)
    report['checks'].append('Catalogue flying presentation captured for visual review')
    report['passed'] = True
except Exception:
    report['error'] = traceback.format_exc()
    raise
finally:
    (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    if proc.poll() is None:
        try: cmd('quit'); proc.wait(timeout=60)
        except Exception: pass
