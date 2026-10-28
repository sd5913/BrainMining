"""Opt-in packaged checkpoints; fresh profile, real timers, screenshots for review."""
import argparse
import json
import re
import subprocess
import time
import traceback
from pathlib import Path
from packaged_qa_client import Client

p = argparse.ArgumentParser()
p.add_argument('output')
p.add_argument('--exe', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
out = (root / a.output).resolve()
profile = out / 'profile'
assert not profile.exists()
out.mkdir(parents=True, exist_ok=True)
config = profile / 'Saved/Config/Windows/GameUserSettings.ini'
config.parent.mkdir(parents=True)
config.write_text('[BrainMining.Localization]\nLanguagePreference=zh-Hans\n', encoding='utf8')
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
process = subprocess.Popen([str(Path(a.exe).resolve()), '-windowed', '-ResX=1280', '-ResY=720',
    '-RenderOffscreen', '-unattended', '-NoSplash', '-MiningQA', '-UserDir=' + str(profile),
    '-abslog=' + str(out / 'game.log')], startupinfo=startup)
c = Client(profile)
report = {'passed': False, 'checks': [], 'office_samples': []}

def cmd(op, **kw):
    result = c.command(op, timeout=180, **kw)
    assert result['ok'], (op, result)
    return result

def check(value, label):
    assert value, label
    report['checks'].append(label)
    print('PASS ' + label, flush=True)

def wait(predicate, seconds=120):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        s = cmd('state')
        if predicate(s):
            return s
        time.sleep(.1)
    raise AssertionError('Timeout: ' + str(s))

def shot(name):
    cmd('shot', name=name)
    time.sleep(.25)

def actors(name, parts=False):
    return cmd('actors', filter=name, parts=parts)['actors']

def elapsed(s):
    return float(re.search(r'elapsed=([0-9.]+)', s['office']).group(1))

def metrics(label):
    result = cmd('sample_end')
    values = sorted(result.pop('frame_ms'))
    return {'label': label, 'frames': len(values), 'p95_ms': values[int((len(values)-1)*.95)],
        'p99_ms': values[int((len(values)-1)*.99)], 'max_ms': max(values),
        'mean_ms': sum(values)/len(values), 'over_50ms': sum(v > 50 for v in values)}

try:
    wait(lambda s: bool(s['menu_buttons']))
    cmd('start')
    wait(lambda s: not s['opening'])
    cmd('gm_tool', tool=0)
    cmd('area_fixture', area=1)
    cmd('office')
    devices = actors('PopupOfficeDevice')
    device = devices[0]
    pos, forward = device['position'], device['forward']
    eye = [pos[i]-forward[i]*140+(145 if i == 2 else 0) for i in range(3)]
    target = [pos[i]+forward[i]*15+(140 if i == 2 else 0) for i in range(3)]
    cmd('pose', eye=eye, target=target)
    c.key('E')
    state = wait(lambda s: elapsed(s) >= 35)
    report['office_samples'].append(state)
    shot('office-middle')
    state = wait(lambda s: elapsed(s) >= 58)
    cmd('sample_start', label='late-desktop')
    state = wait(lambda s: elapsed(s) >= 64.5)
    report['office_samples'].append(state)
    desktops = state['office_desktops']
    check(len(desktops)==2 and all(d['windows_inside_screen'] for d in desktops), 'Every late popup stays completely inside the CRT safe area')
    check(all(d['pending'] >= 45 for d in desktops), 'Late traffic builds a dense backlog without a forced screen coverage target')
    check(all(d['accounting_valid'] for d in desktops), 'Denser traffic preserves issue accounting')
    report['late_desktop_performance'] = metrics('late-desktop')
    shot('office-filled')
    wait(lambda s: elapsed(s) >= 65)
    time.sleep(2)
    shot('office-eruption')
    cmd('sample_start', label='prewarmed-eruption')
    time.sleep(10)
    state = cmd('state')
    report['eruption_performance'] = metrics('prewarmed-eruption')
    report['office_samples'].append(state)
    check('pool=240/240' in state['office'], 'Physical eruption reuses the fully prepared 240-object pool')
    check('missing=0 compiling=0 pso=0' in state['office'], 'Physical popup materials and pipelines are ready during eruption')

    cmd('area_fixture', area=3)
    toy = next(x for x in actors('CardRoomRobot') if 'CardRoomFloorRobot' in x['tags'])
    cmd('pose', actor=toy['name'], bounds=True, offset=[-2400, 900, 850])
    shot('floor-robot')
    stage = actors('CardRoomStage')[0]
    pos = stage['position']
    cmd('pose', eye=[pos[0]-75, pos[1], pos[2]+115], target=pos)
    cmd('card', action='interact')
    wait(lambda s: s['card_phase'] == 4)
    for turn in range(1, 5):
        state = cmd('state')
        check(state['card_round'] == turn, 'Reached card round ' + str(turn))
        if turn == 4:
            check(len(state['card_hand']) == 17 and len(set(state['card_hand'])) == 17, 'Final hand has seventeen unique cards')
            shot('seventeen-elements')
        cmd('card', action='all')
        cmd('card', action='confirm')
        if turn == 4:
            state = wait(lambda s: s['card_time'] >= .8)
            check(state['card_rival_hp'] == 28, 'Fusion animation precedes final damage')
            check('十七重' in state['subtitle'], 'Long fusion skill name appears in subtitles')
            shot('fusion-start')
            wait(lambda s: s['card_time'] >= 1.6)
            shot('fusion-orbit')
            wait(lambda s: s['card_time'] >= 2.5)
            shot('fusion-collapse')
            wait(lambda s: s['card_time'] >= 3.5)
            shot('fusion-impact')
        wait(lambda s: s['card_phase'] in (4, 6))
    check(cmd('state')['card_phase'] == 6, 'Fusion attack reaches victory normally')
    cmd('card', action='interact')

    cmd('area_fixture', area=4)
    c.key('C')
    cmd('catalogue_owned_fixture', item=21)
    cmd('catalogue_rotate', item=21, yaw=0, pitch=0)
    wait(lambda s: s['catalogue_preview_visible'])
    time.sleep(1)
    shot('subway-white-green')
    cmd('catalogue_rotate', item=21, yaw=35, pitch=10)
    shot('subway-neutral-crowd')
    c.key('C')
    cmd('area_fixture', area=5)
    cmd('mouse_gallery_fixture')
    portal = actors('MouseMuseumPortal')[0]
    cell = portal['cell']
    eye = [cell[0]-110, cell[1], cell[2]+8]
    cmd('pose', eye=eye, target=[cell[0], cell[1]+14, cell[2]])
    actual = cmd('state')['camera']
    cmd('pose', eye=[2*eye[i]-actual[i] for i in range(3)], target=[cell[0], cell[1]+14, cell[2]])
    time.sleep(.4)
    portal = actors('MouseMuseumPortal', True)[0]
    check(not portal['focused'], 'Breathing marker is reviewed before aiming at the circle')
    canvas = next(x for x in portal['parts'] if x['name'] == 'MouseKissCanvas')
    check(canvas['highlight'] > 0, 'Painting marker remains visible without precise aim')
    for index in range(4):
        shot('portal-breath-' + str(index))
        time.sleep(.4)
    report['passed'] = True
except Exception:
    report['error'] = traceback.format_exc()
    print(report['error'], flush=True)
finally:
    if process.poll() is None:
        try:
            cmd('quit')
            process.wait(timeout=60)
        except Exception:
            process.terminate()
            process.wait(timeout=15)
    report['exit_code'] = process.returncode
    (out / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
if not report['passed']:
    raise SystemExit(1)
