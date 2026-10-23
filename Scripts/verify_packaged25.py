"""Shipping guide regression using real terrain excavation and pickup APIs."""
import argparse
import json
from pathlib import Path
import subprocess
import time
from packaged_qa_client import Client

parser = argparse.ArgumentParser()
parser.add_argument('exe')
parser.add_argument('output')
args = parser.parse_args()
exe = Path(args.exe).resolve()
root = Path(args.output).resolve()
checks = []


def check(value, label):
    assert value, label
    checks.append(label)
    print('PASS ' + label, flush=True)


def launch(user):
    process = subprocess.Popen([str(exe), '-windowed', '-ResX=1600', '-ResY=900',
        '-NoSplash', '-MiningQA', '-UserDir=' + str(user)])
    client = Client(user)
    client.command('state', timeout=90)
    return process, client


def close(process, client):
    client.command('quit')
    process.wait(timeout=90)


def upgrade_visible(state):
    return next(r['visible'] for r in state['guide_rows'] if r['index'] == 2)


def pickup(client, mineral, expected_before):
    items = client.command('actors', filter='MiningTreasure')['actors']
    item = next(a for a in items if a.get('mineral') == mineral and
                -30000 < a['position'][0] < -18000 and not a['collected'])
    point = item['position']
    client.command('pose', eye=[point[0]-90, point[1], point[2]+35], target=point)
    time.sleep(.3)
    check(not client.command('treasure')['ok'], 'buried ore cannot be collected: ' + mineral)
    for host in item['hosts']:
        client.command('mine', actor=host, point=point, radius=300)
        time.sleep(1)
    time.sleep(2)
    item = next(a for a in client.command('actors', filter='MiningTreasure')['actors'] if a['path'] == item['path'])
    check(item['released'], 'actual excavation releases ore: ' + mineral)
    state = client.command('state')
    check(state['completed'] == expected_before and not upgrade_visible(state),
          'exposure alone does not complete collection or reveal upgrade: ' + mineral)
    point = item['position']
    client.command('pose', eye=[point[0]-90, point[1], point[2]+35], target=point)
    time.sleep(.2)
    client.command('aim', target=point)
    time.sleep(.2)
    check(client.command('treasure')['ok'], 'actual pickup succeeds: ' + mineral)


for order in [('2', '4'), ('4', '2')]:
    user = root / ('copper-first' if order[0] == '2' else 'iron-first')
    check(not (user/'Saved'/'SaveGames').exists(), 'fresh isolated test directory ' + user.name)
    process, client = launch(user)
    deadline = time.monotonic() + 30
    while client.command('state')['opening']:
        assert time.monotonic() < deadline, 'opening timeout'
        client.command('start')
        time.sleep(.5)
    for item in client.command('actors', filter='StarterEquipment')['actors']:
        client.command('pose', actor=item['path'], offset=[-125, 0, 50])
        time.sleep(.3)
        check(client.command('interact')['ok'], 'starter equipment pickup')
    client.command('pose', target=[-30150, 0, 12140], offset=[-450, 0, 40])
    time.sleep(3)
    state = client.command('state')
    check([r['text'] for r in state['guide_rows'] if r['visible']] == ['·  穿过1个泥土层'], 'soil is the only first objective')
    soil = next(a for a in client.command('actors', filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(client.command('mine', actor=soil['path'], point=soil['position'], radius=230)['ok'], 'excavate tutorial soil')
    time.sleep(2)
    check(not client.command('state')['soil_passed'], 'excavation alone does not complete crossing')
    client.command('pose', eye=[-29850, 0, 12180], target=[-29000, 0, 12180])
    time.sleep(3)
    state = client.command('state')
    check(state['soil_passed'], 'crossing unlocks later objectives')
    check(state['tutorial'] and not upgrade_visible(state), 'initial guide hides upgrade')
    check([r['index'] for r in state['guide_rows'] if r['visible']] == [0, 1, 3, 4], 'four initial objectives')
    check('收集1个铜矿' in state['guide_rows'][0]['text'] and '收集1个铁矿' in state['guide_rows'][1]['text'], 'collection wording')
    client.command('shot', name='01-initial-guide')
    # Wallet rewards are deliberately not evidence of a physical collection.
    client.command('mineral_fixture', mineral='2', amount=4)
    client.command('mineral_fixture', mineral='4', amount=2)
    time.sleep(1)
    check(client.command('state')['completed'] == 0, 'wallet changes do not complete collection tasks')
    pickup(client, order[0], 0)
    time.sleep(2)
    first_bit = 1 if order[0] == '2' else 2
    state = client.command('state')
    check(state['completed'] == first_bit and not upgrade_visible(state), 'one collected mineral keeps upgrade hidden')
    check(client.command('save')['ok'], 'save after first mineral')
    close(process, client)
    process, client = launch(user)
    client.command('continue')
    time.sleep(3)
    state = client.command('state')
    check(not state['opening'] and state['completed'] == first_bit and not upgrade_visible(state), 'first collection persists across restart')
    pickup(client, order[1], first_bit)
    samples = []
    for _ in range(14):
        samples.append(client.command('state'))
        time.sleep(.025)
    check(samples[-1]['completed'] == 3 and upgrade_visible(samples[-1]), 'both pickups reveal upgrade')
    left = [s['guide_opacity'] for s in samples]
    right = [s['upgrade_opacity'] for s in samples]
    check(max(left)-min(left) > .25 and max(right)-min(right) > .25, 'guide and upper-right panel both animate')
    check(all(abs(a-b) < .02 for a, b in zip(left, right)), 'attention pulses are synchronized')
    (client.directory/'pulse-samples.json').write_text(json.dumps(samples, ensure_ascii=False, indent=2), encoding='utf-8')
    client.command('shot', name='02-upgrade-revealed')
    time.sleep(3)
    check(client.command('state')['upgrade_opacity'] == 1, 'flash finishes at full opacity')
    check(client.command('upgrade')['ok'], 'real purchase succeeds')
    time.sleep(2)
    state = client.command('state')
    check(state['completed'] == 7 and not upgrade_visible(state), 'upgrade completes and disappears')
    client.command('shot', name='03-upgraded')
    check(client.command('save')['ok'], 'save completed objectives')
    close(process, client)
    process, client = launch(user)
    client.command('continue')
    time.sleep(3)
    state = client.command('state')
    check(state['completed'] == 7 and state['upgrade'] == 1 and not upgrade_visible(state), 'completed upgrade persists across restart')
    check(state['upgrade_opacity'] == 1, 'completed upgrade does not flash again after load')
    close(process, client)

(root/'results.json').write_text(json.dumps(dict(passed=True, checks=checks), ensure_ascii=False, indent=2), encoding='utf-8')
