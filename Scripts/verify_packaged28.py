"""Test main-menu pointer events through Slate hit testing in Shipping."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import time
from packaged_qa_client import Client

parser=argparse.ArgumentParser()
parser.add_argument('exe')
parser.add_argument('output')
parser.add_argument('save_directory',help='Existing isolated QA save fixture; copied, never changed')
args=parser.parse_args()
root=Path(args.output).resolve()
user=root/'profile'
assert not user.exists(),'Use a fresh output directory'
shutil.copytree(args.save_directory,user/'Saved'/'SaveGames')
checks=[]
process=subprocess.Popen([str(Path(args.exe).resolve()),'-windowed','-ResX=1600','-ResY=900',
    '-NoSplash','-MiningQA','-UserDir='+str(user)])
client=Client(user)


def check(value,label):
    assert value,label
    checks.append(label);print('PASS '+label,flush=True)


def button(action):
    name='Opening'+action.capitalize()+'Button'
    return next(row for row in client.command('state')['menu_buttons'] if row['name']==name)


def pointer(action,phase='move'):
    check(client.command('menu_pointer',action=action,phase=phase)['ok'],phase+' pointer: '+action)


try:
    deadline=time.monotonic()+90
    while not client.command('state',timeout=90)['menu_buttons']:
        assert time.monotonic()<deadline,'menu timeout'
        time.sleep(.2)
    pointer('away');time.sleep(.8)
    row=button('continue')
    check(row['focused'] and not row['hovered'] and abs(row['scale']-1)<.005,'default Continue focus does not pre-expand hover state')
    client.command('shot',name='01-idle')
    for action in ('continue','start','exit'):
        before=button(action)
        pointer(action)
        samples=[]
        for _ in range(5):samples.append(button(action));time.sleep(.08)
        row=samples[-1]
        check(row['hovered'] and abs(row['scale']-1.12)<.01,'actual hover visibly enlarges '+action)
        check(row['scale']-before['scale']>.1,'hover changes from idle by more than ten percent: '+action)
        check(row['angle']==before['angle'] and row['shear']==before['shear'],'hover preserves perspective: '+action)
        if action=='continue':client.command('shot',name='02-continue-hover')
        pointer(action,'down');time.sleep(.5)
        row=button(action)
        check(row['pressed'] and abs(row['scale']-.96)<.01,'held pointer press provides depression feedback: '+action)
        if action=='continue':client.command('shot',name='03-continue-pressed')
        pointer('away');pointer('away','up');time.sleep(.7)
        row=button(action)
        check(not row['hovered'] and not row['pressed'] and abs(row['scale']-1)<.005,'pointer leave returns '+action+' to normal size')
        check(client.command('state')['opening'],'release outside cancels click: '+action)
    client.command('shot',name='04-after-leave')
    pointer('continue');time.sleep(.3);pointer('continue','down');pointer('continue','up')
    time.sleep(1)
    state=client.command('state')
    check(not state['opening'] and not state['menu_buttons'],'real pointer click dismisses menu and continues')
    check(state['upgrade']>0 and state['soil_passed'],'pointer click restores existing progress')
    (root/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),'utf-8')
finally:
    if process.poll() is None:
        client.command('quit');process.wait(timeout=90)
