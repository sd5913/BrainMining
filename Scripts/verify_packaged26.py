"""Check the single soil objective and restart persistence in the Shipping package.

Uses explicit position checkpoints and actual terrain cuts; not a manual playthrough.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
from packaged_qa_client import Client

parser = argparse.ArgumentParser()
parser.add_argument('exe')
parser.add_argument('output')
parser.add_argument('--previous-exe', help='Optional previous Shipping release for real version-1 save migration checks')
args = parser.parse_args()
exe = Path(args.exe).resolve()
root = Path(args.output).resolve()
user = root/'soil'
assert not (user/'Saved'/'SaveGames').exists(), 'Use a fresh independent user directory'
checks = []
process = None


def check(value, label):
    assert value, label
    checks.append(label)
    print('PASS ' + label, flush=True)


def launch(binary=None):
    global process, client
    process = subprocess.Popen([str(binary or exe), '-windowed', '-ResX=1600', '-ResY=900',
        '-NoSplash', '-MiningQA', '-UserDir=' + str(user)])
    client = Client(user)
    client.command('state', timeout=90)


def close():
    client.command('quit')
    process.wait(timeout=90)


def pose(x, y=0):
    client.command('pose', eye=[x, y, 12180], target=[x+1000, y, 12180])
    time.sleep(.7)
    return client.command('state')


def visible(state):
    return [row['text'] for row in state['guide_rows'] if row['visible']]


try:
    launch()
    deadline = time.monotonic()+30
    while client.command('state')['opening']:
        assert time.monotonic()<deadline, 'opening timeout'
        client.command('start')
        time.sleep(.5)
    check(not client.command('state')['tutorial'], 'no guide in lounge')
    for item in client.command('actors', filter='StarterEquipment')['actors']:
        client.command('pose', actor=item['path'], offset=[-125,0,50])
        time.sleep(.3)
        check(client.command('interact')['ok'], 'starter equipment pickup')
    state = pose(-30600)
    check(state['tutorial'] and not state['soil_passed'], 'second soil activates incomplete first task')
    check(visible(state)==['·  穿过1个泥土层'], 'only crossing task appears')
    samples = [client.command('state') for _ in range(12)]
    check(max(s['guide_opacity'] for s in samples)-min(s['guide_opacity'] for s in samples)>.25, 'first task flashes')
    check(all('软软的泥土' not in s['subtitle'] for s in samples), 'discovery subtitle withheld before crossing')
    client.command('shot', name='01-soil-task')
    check(client.command('save')['ok'], 'save before excavation')
    close(); launch(); client.command('continue'); time.sleep(2)
    state = client.command('state')
    check(not state['soil_passed'] and visible(state)==['·  穿过1个泥土层'], 'restart retains incomplete soil task')
    soil = next(a for a in client.command('actors', filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(client.command('mine', actor=soil['path'], point=soil['position'], radius=230)['ok'], 'actual terrain excavation succeeds')
    time.sleep(2)
    check(not client.command('state')['soil_passed'], 'excavation while in front does not count')
    check(client.command('save')['ok'], 'save excavated but not crossed soil')
    close(); launch(); client.command('continue'); time.sleep(2)
    check(not client.command('state')['soil_passed'], 'restart after excavation still requires crossing')
    restored = next(a for a in client.command('actors', filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(restored['cuts']>0, 'excavated soil restored')
    check(not pose(-29850,700)['soil_passed'], 'outside this soil corridor does not count')
    check(not pose(-30100)['soil_passed'], 'standing inside soil thickness does not count')
    state = pose(-29850)
    check(state['soil_passed'], 'clearing the far side completes task')
    check(state['subtitle']=='哦！软软的泥土，这边一定是路', 'completion plays exact new discovery line')
    check(visible(state)==['✓  穿过1个泥土层'], 'completed soil task shows checkmark before replacement')
    client.command('shot', name='02-soil-complete')
    time.sleep(2)
    state = client.command('state')
    check(visible(state)==['·  收集1个铜矿','·  收集1个铁矿','·  找到1个灵感藏品','·  发现1个灵感原型'], 'later tasks appear after crossing, upgrade stays hidden')
    client.command('shot', name='03-following-tasks')
    check(client.command('save')['ok'], 'save completed crossing')
    close(); launch(); client.command('continue'); time.sleep(3)
    state = client.command('state')
    check(state['soil_passed'] and all('穿过' not in text for text in visible(state)), 'completed crossing survives process restart')
    check('软软的泥土' not in state['subtitle'], 'loading completion does not replay subtitle')
    pose(-30600); time.sleep(4); state=pose(-29850)
    check('软软的泥土' not in state['subtitle'], 'backtracking does not repeat discovery subtitle')
    close()
    if args.previous_exe:
        for passed in (False, True):
            user = root/('legacy-past-soil' if passed else 'legacy-before-soil')
            assert not (user/'Saved'/'SaveGames').exists(), 'Use fresh legacy fixtures'
            launch(Path(args.previous_exe).resolve())
            deadline=time.monotonic()+30
            while client.command('state')['opening']:
                assert time.monotonic()<deadline, 'legacy opening timeout'
                client.command('start'); time.sleep(.5)
            pose(-30600)
            if passed:
                soil = next(a for a in client.command('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
                check(client.command('mine',actor=soil['path'],point=soil['position'],radius=230)['ok'],'legacy terrain excavation')
                time.sleep(2); pose(-29850)
            check(client.command('save')['ok'],'create real previous-release save: '+str(passed))
            close(); launch(); client.command('continue'); time.sleep(3)
            state=client.command('state')
            check(not state['opening'] and state['soil_passed']==passed,'version-1 soil stage migrates: '+str(passed))
            check(any('穿过1个泥土层' in text for text in visible(state))!=passed,'legacy save displays appropriate objectives: '+str(passed))
            check('哦！软软的泥土' not in state['subtitle'],'legacy load does not replay completion: '+str(passed))
            close()
    (root/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf-8')
finally:
    if process and process.poll() is None:
        close()
