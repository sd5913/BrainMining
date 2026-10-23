"""Run checkpoints against the actual Shipping executable, never PIE."""
import argparse
import json
import time
from packaged_qa_client import Client

parser = argparse.ArgumentParser()
parser.add_argument('user_dir')
parser.add_argument('phase', choices=['setup', 'office', 'cards', 'escape', 'resume'])
args = parser.parse_args()
c = Client(args.user_dir)
checks = []
def check(value, label):
    assert value, label
    checks.append(label)
    print('PASS ' + label, flush=True)
def command(op, **kw):
    r = c.command(op, **kw)
    check(r['ok'], op + ' ' + str(kw))
    return r
def actors(text):
    return c.command('actors', filter=text)['actors']

if args.phase == 'setup':
    state=c.command('state')
    if state['opening']:
        command('start')
        deadline=time.monotonic()+30
        while c.command('state')['opening']:
            assert time.monotonic()<deadline, 'opening did not finish before checkpoint setup'
            c.command('start')  # Safe if already started; initial widget may still be preparing.
            time.sleep(.5)
    check(not c.command('state')['tutorial'], 'guide absent in starting lounge')
    for pickup in actors('StarterEquipment'):
        command('pose', actor=pickup['name'], offset=[-125,0,50]);time.sleep(.4)
        command('interact')
    command('pose', target=[-30150,0,12140], offset=[-450,0,40]);time.sleep(.7)
    check(c.command('state')['tutorial'], 'second soil activates guide')
    command('shot', name='tutorial-start')
    # Real terrain mutation is retained across process restarts.
    command('mine', actor='BM_StartRoom_ExitBlocker', point=[-33730,0,11225],radius=120)
    time.sleep(3)
    rock=next(a for a in actors('BM_StartRoom_ExitBlocker') if 'cuts' in a)
    check(rock['cuts']>0,'starter wall excavation committed')
    (c.directory/'terrain-expected.json').write_text(json.dumps(rock),encoding='utf-8')
    # Collection objectives now require successful physical pickups.
    for mineral in ('2','4'):
        item=next(a for a in actors('MiningTreasure') if a.get('mineral')==mineral and -30000<a['position'][0]<-18000 and not a['collected'])
        point=item['position']
        command('pose',eye=[point[0]-150,point[1],point[2]+70],target=point)
        for host in item['hosts']:
            c.command('mine',actor=host,point=point,radius=300);time.sleep(1)
        time.sleep(2)
        item=next(a for a in actors('MiningTreasure') if a['path']==item['path']);point=item['position']
        command('pose',eye=[point[0]-90,point[1],point[2]+35],target=point);time.sleep(.2)
        command('aim',target=point);time.sleep(.2);command('treasure')
    command('mineral_fixture',mineral='2',amount=4)
    command('mineral_fixture',mineral='4',amount=2)
    time.sleep(.6)
    check(c.command('state')['completed'] & 3 == 3,'copper and iron pickups complete objectives')
    command('shot',name='tutorial-ore-checks')
    command('upgrade');time.sleep(2)
    r=c.command('state')
    check(r['upgrade']==1 and r['completed'] & 7 == 7,'purchase spends ore and preserves completed objectives')
    command('shot',name='tutorial-after-upgrade')
    command('save')

elif args.phase=='office':
    command('area_fixture',area=1);time.sleep(2)
    check(c.command('state')['prototypes']==1,'office entry discovers prototype one')
    device=actors('PopupOfficeDevice')[0]
    target=[device['position'][i]+(124 if i==2 else 0) for i in range(3)]
    eye=[device['position'][i]-device['forward'][i]*215+(158 if i==2 else 0) for i in range(3)]
    command('pose',eye=eye,target=target);time.sleep(3)
    command('office')
    print(json.dumps(c.sample('office-running',20)['metrics']),flush=True)
    c.key('E');time.sleep(1)
    command('shot',name='office-screen')
    print(json.dumps(c.sample('office-screen',15)['metrics']),flush=True)
    c.key('E');time.sleep(1)
    # Let the original 65-second round and 15-second eruption run in real time.
    while 'phase=1 ' in c.command('state')['office']:
        time.sleep(2)
    command('shot',name='office-burst')
    print(json.dumps(c.sample('office-eruption',12)['metrics']),flush=True)
    print(c.command('state')['office_exit'],flush=True)

elif args.phase=='cards':
    command('area_fixture',area=3);time.sleep(2)
    check(c.command('state')['prototypes']>=2,'blind passage discovers prototype two')
    stage=actors('CardRoomStage')[0]
    # Stage axes in this campaign are unrotated; entrance is the authored seat volume.
    target=stage['position'];eye=[target[0]-75,target[1],target[2]+115]
    command('pose',eye=eye,target=target);time.sleep(2)
    check(c.command('state')['prototypes']==3,'entering card room discovers prototype three')
    command('shot',name='card-entrance')
    command('card',action='interact');time.sleep(.5)
    command('card',action='interact');time.sleep(5)
    for round in range(1,5):
        r=c.command('state')
        check(r['card_round']==round and r['card_phase']==4,'round '+str(round)+' awaiting real selection')
        command('card',action='all')
        command('sample_start',label='cards-round-'+str(round))
        command('card',action='confirm')
        # Screenshot readback stalls the render thread; capture outside timed samples.
        time.sleep(5.2)
        r=c.command('sample_end')
        (c.directory/('cards-round-'+str(round)+'.json')).write_text(json.dumps(r,ensure_ascii=False),encoding='utf-8')
    check(c.command('state')['card_phase'] in (6,7),'four rounds reach victory without forcing state')
    command('card',action='interact')
    check(c.command('state')['drill'],'victory reward unlocks the drill')
    command('save')

elif args.phase=='escape':
    state=c.command('state')
    check(state['drill'],'card reward owned before escape')
    checkpoint=state['tv_checkpoint_position']
    command('pose',eye=[checkpoint[0],checkpoint[1],checkpoint[2]+64],target=state['tv_entry'])
    time.sleep(6)
    state=c.command('state')
    check(state['tv_checkpoint'],'television shelf checkpoint reached')
    rocks={a['path']:a for a in c.command('actors',filter='DiggableRock')['actors']}
    for index,path in enumerate(state['computer_rocks']):
        rock=rocks[path]
        command('mine',actor=path,point=rock['center'],radius=500)
        time.sleep(.6)
        if index==0:
            entry=state['tv_entry']
            position=[entry[0]+250,entry[1],entry[2]+133]
        else:
            position=[rock['position'][i]+rock['forward'][i]*(rock['rock_size'][0]/2+100) for i in range(3)]
        command('pose',eye=[position[0],position[1],position[2]+64],target=[position[0]+500,position[1],position[2]+300])
        time.sleep(.6)
        check(c.command('state')['nodes']==index+1,'actual cut and crossing node '+str(index))
    exit=state['journey_exit']
    command('pose',eye=[exit[0],exit[1],exit[2]+64],target=[exit[0]+500,exit[1],exit[2]+64])
    time.sleep(1)
    check(c.command('state')['journey_phase']==4,'escape reaches completion after all mined sections')
    command('shot',name='escape-complete')
    command('save')

elif args.phase=='resume':
    check(c.command('state')['has_save'],'continue available after process restart')
    command('continue');time.sleep(3)
    state=c.command('state')
    check(not state['opening'],'continue releases opening camera')
    check(state['tutorial'] and state['completed'] & 7 == 7,'tutorial and upgrade persist after process restart')
    check(state['upgrade']==1,'purchased pickaxe level persists')
    expected=json.loads((c.directory/'terrain-expected.json').read_text(encoding='utf-8'))
    rock=next(a for a in actors('BM_StartRoom_ExitBlocker') if 'cuts' in a)
    check(rock['cuts']==expected['cuts'] and rock['triangles']==expected['triangles'],'edited terrain topology persists')
    command('shot',name='resume')

(c.directory/(args.phase+'-checks.json')).write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
