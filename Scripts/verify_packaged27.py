"""Exercise actual main-menu button delegates in an isolated Shipping profile."""
import argparse
import json
from pathlib import Path
import subprocess
import time
from packaged_qa_client import Client

parser=argparse.ArgumentParser()
parser.add_argument('exe')
parser.add_argument('output')
args=parser.parse_args()
exe=Path(args.exe).resolve()
root=Path(args.output).resolve()
user=root/'profile'
assert not user.exists(), 'Use a fresh isolated profile'
checks=[]
process=None


def check(value,label):
    assert value,label
    checks.append(label)
    print('PASS '+label,flush=True)


def launch(width=1600,height=900):
    global process,client
    process=subprocess.Popen([str(exe),'-windowed',f'-ResX={width}',f'-ResY={height}',
        '-NoSplash','-MiningQA','-UserDir='+str(user)])
    client=Client(user)
    deadline=time.monotonic()+90
    while True:
        state=client.command('state',timeout=90)
        if state['menu_buttons']:
            time.sleep(1)
            return client.command('state')
        assert time.monotonic()<deadline,'menu timeout'
        time.sleep(.2)


def close():
    client.command('quit')
    process.wait(timeout=90)


def click(action):
    check(client.command('menu_click',action=action)['ok'],'actual '+action+' button click')
    deadline=time.monotonic()+20
    while client.command('state')['opening']:
        assert time.monotonic()<deadline,'opening transition timeout'
        time.sleep(.2)
    time.sleep(.8)
    state=client.command('state')
    check(not state['menu_buttons'],'all menu buttons disappear after '+action)
    return state


def menu(state,has_save):
    rows=state['menu_buttons']
    expected=(['继续游戏'] if has_save else [])+['开始游戏','退出游戏']
    check([r['text'] for r in rows if r['visible']]==expected,'menu choices match save availability: '+str(has_save))
    check(all(abs(r['angle'])>0 and abs(r['shear'])>0 for r in rows),'every button retains tilted perspective')
    check(all(b['position'][1]>a['position'][1]+30 for a,b in zip(rows,rows[1:])),'buttons are ordered vertically without overlap')
    check(rows[0]['focused'] and rows[0]['scale']>1.02,'preferred action has focus and matching focus animation')


try:
    state=launch();menu(state,False)
    initial_positions={r['text']:r['position'] for r in state['menu_buttons']}
    client.command('shot',name='01-no-save')
    check(not client.command('menu_click',action='continue')['ok'],'continue is unavailable without a save')
    click('start')
    for item in client.command('actors',filter='StarterEquipment')['actors']:
        client.command('pose',actor=item['path'],offset=[-125,0,50]);time.sleep(.3)
        check(client.command('interact')['ok'],'collect starter equipment')
    client.command('mineral_fixture',mineral='2',amount=9)
    client.command('mineral_fixture',mineral='4',amount=5)
    check(client.command('upgrade')['ok'],'purchase upgrade for recognizable progress')
    client.command('pose',eye=[-30600,0,12180],target=[-30150,0,12140]);time.sleep(1)
    soil=next(a for a in client.command('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(client.command('mine',actor=soil['path'],point=soil['position'],radius=230)['ok'],'excavate recognizable terrain')
    time.sleep(2)
    client.command('pose',eye=[-29850,0,12180],target=[-29000,0,12180]);time.sleep(2)
    expected=client.command('state')
    check(expected['soil_passed'] and expected['upgrade']==1,'test profile has saved tutorial and upgrade progress')
    check(client.command('save')['ok'],'save progress before returning to menu')
    close()

    state=launch();menu(state,True)
    for row in state['menu_buttons']:
        if row['text'] in initial_positions:
            check(abs(row['position'][1]-initial_positions[row['text']][1])<8,'existing '+row['text']+' keeps its vertical position')
    client.command('shot',name='02-with-save')
    state=click('continue')
    check(all(state[key]==expected[key] for key in ('copper','iron','upgrade','completed','soil_passed','pickaxe','jetpack')),'Continue restores inventory, upgrades, equipment and tutorial')
    rock=next(a for a in client.command('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(rock['cuts']>0,'Continue restores excavated terrain')
    check(abs(state['position'][0]-expected['position'][0])<5,'Continue restores saved location')
    close()

    state=launch(1280,960);menu(state,True)
    client.command('shot',name='03-with-save-4x3')
    state=click('start')
    check(state['copper']==0 and state['iron']==0 and state['upgrade']==0,'Start with an existing save begins with fresh inventory and upgrade')
    check(not state['tutorial'] and not state['soil_passed'] and state['completed']==0,'Start resets tutorial progress')
    check(not state['pickaxe'] and not state['jetpack'],'Start resets starter equipment')
    rock=next(a for a in client.command('actors',filter='BM_MainGame_StartRoomJunction')['actors'] if 'rock_size' in a)
    check(rock['cuts']==0,'Start leaves terrain intact')
    check(state['position'][0]<-33000,'Start returns to the starting lounge')
    check(client.command('save')['ok'],'new game progress can replace old progress')
    close()

    state=launch();menu(state,True);state=click('continue')
    check(state['upgrade']==0 and state['copper']==0 and not state['soil_passed'],'Continue uses the new game after it is saved')
    close()

    user=root/'invalid-save'
    slots=user/'Saved'/'SaveGames';slots.mkdir(parents=True)
    (slots/'BrainMiningProgress_v1.sav').write_bytes(b'invalid isolated test save')
    state=launch();menu(state,False)
    check(not state['has_save'],'invalid save does not advertise Continue')
    close()
    (root/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),'utf-8')
finally:
    if process and process.poll() is None:close()
