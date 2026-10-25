"""Polish29 Shipping regression. Uses only a new isolated profile and actual gameplay APIs."""
import argparse,json,time,subprocess
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');args=ap.parse_args()
exe=Path(args.exe).resolve();out=Path(args.output).resolve();user=out/'profile'
assert not user.exists(),'Use a fresh test profile'
checks=[];process=None

def check(value,label):
 assert value,label
 checks.append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
 r=c.command(op,**kw);check(r['ok'],op+' '+str(kw));return r
def state():return c.command('state')
def wait(pred,seconds=40):
 end=time.monotonic()+seconds
 while True:
  s=state()
  if pred(s):return s
  assert time.monotonic()<end,'condition timeout '+str(s)
  time.sleep(.2)
def launch():
 global c,process
 process=subprocess.Popen([str(exe),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(user)])
 c=Client(user);c.command('state',timeout=100);return wait(lambda s:bool(s['menu_buttons']))
def close():
 if process and process.poll() is None:c.command('quit');process.wait(timeout=60)
def rows(s):return [r['text'] for r in s.get('guide_rows',[]) if r['visible']]
def menu():
 cmd('return_menu');s=wait(lambda s:s['opening'] and bool(s['menu_buttons']),100);time.sleep(.5);return s
def start(keep=None):
 cmd('menu_click',action='start')
 if keep is not None:
  s=wait(lambda s:s['collection_choice']);check(s['opening'],'confirmation blocks new game until choice')
  cmd('shot',name='collection-choice-'+str(keep));cmd('confirm_new',keep=keep)
 s=wait(lambda s:not s['opening']);time.sleep(1);return state()
def resume():
 cmd('menu_click',action='continue',timeout=120);s=wait(lambda s:not s['opening']);time.sleep(1);return state()
def actors(f):return c.command('actors',filter=f)['actors']
def pickup_ore(mineral):
 item=next(a for a in actors('MiningTreasure') if a.get('mineral')==mineral and -30000<a['position'][0]<-18000 and not a['collected'])
 point=item['position'];before=state()['completed']
 cmd('pose',eye=[point[0]-90,point[1],point[2]+35],target=point);time.sleep(.3)
 check(not c.command('treasure')['ok'],'buried '+mineral+' cannot count as pickup')
 for host in item['hosts']:c.command('mine',actor=host,point=point,radius=300);time.sleep(.8)
 time.sleep(2)
 item=next(a for a in actors('MiningTreasure') if a['path']==item['path']);check(item['released'],'ore '+mineral+' released by excavation')
 check(state()['completed']==before,'exposing '+mineral+' alone does not finish collection')
 point=item['position'];cmd('pose',eye=[point[0]-90,point[1],point[2]+35],target=point);time.sleep(.3);cmd('aim',target=point);time.sleep(.2);cmd('treasure')
def pickup_bird():
 cmd('area_fixture',area=2);time.sleep(2)
 bird=next(a for a in actors('MiningCollectible') if a.get('item')==3 and a['position'][2]>0)
 placement=json.loads((Path(__file__).resolve().parent/'polish32_scene_checkpoints.json').read_text(encoding='utf8'))['pigeon']
 check(-3500<bird['position'][0]<-2800 and 11000<bird['position'][2]<11900,'pigeon moved behind blind-passage bomb dead end')
 cmd('pose',eye=placement['approach'],target=placement['clue']);time.sleep(1);cmd('shot',name='pigeon-new-location')
 cmd('gm_tool',tool=1);c.key('G');time.sleep(.4);cmd('throw_bomb');time.sleep(5)
 bird=next(a for a in actors('MiningCollectible') if a.get('item')==3 and a['position'][2]>0)
 check(bird['released'],'new pigeon placement can be excavated')
 point=bird['position'];cmd('pose',eye=[point[0]-95,point[1],point[2]+30],target=point);time.sleep(.3);cmd('aim',target=point);time.sleep(.3);cmd('collect')
 s=state();check(3 in s['run_items'] and s['collection']>=1,'pigeon unlock and this-run pickup both recorded');return s
try:
 s=launch();check([r['text'] for r in s['menu_buttons']]==['开始游戏','退出游戏'],'fresh profile only offers Start')
 s=start();check(s['upgrade']==0 and s['collection']==0,'fresh game starts at level zero')
 check(s['collection_shortcut']=='[C]查看藏品','permanent catalogue shortcut present before tutorial')
 for item in actors('StarterEquipment'):
  cmd('pose',actor=item['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
 exit_rock=next(a for a in actors('BM_StartRoom_ExitBlocker') if 'cuts' in a)
 cmd('mine',actor=exit_rock['path'],point=exit_rock['position'],radius=240);time.sleep(2)
 cmd('pose',eye=[-30600,0,12180],target=[-30150,0,12140]);time.sleep(2)
 check(rows(state())==['·  穿过1个泥土层'],'initial guide is the single soil task')
 soil=next(a for a in actors('BM_MainGame_StartRoomJunction') if 'rock_size' in a)
 cmd('mine',actor=soil['path'],point=soil['position'],radius=230);time.sleep(2)
 check(not state()['soil_passed'],'mining alone does not complete soil task')
 cmd('pose',eye=[-29850,0,12180],target=[-29000,0,12180]);time.sleep(3)
 check(not any('升级' in r for r in rows(state())),'upgrade task remains gated behind real ore pickups')
 pickup_ore('2');time.sleep(1);check(not any('升级' in r for r in rows(state())),'copper alone does not reveal upgrade')
 pickup_ore('4');time.sleep(.5)
 check('·  升级1次矿镐' in rows(state()),'both collected ores reveal first upgrade')
 cmd('mineral_fixture',mineral='2',amount=20);cmd('mineral_fixture',mineral='4',amount=20)
 cmd('upgrade');time.sleep(.7);check('✓  升级1次矿镐' in rows(state()),'first upgrade displays completion check')
 time.sleep(1)
 samples=[state() for _ in range(12)]
 check('·  将矿镐升至2级' in rows(samples[-1]),'second upgrade objective replaces first')
 check(max(s['guide_opacity'] for s in samples)-min(s['guide_opacity'] for s in samples)>.2,'second upgrade objective flashes')
 check(max(s['upgrade_opacity'] for s in samples)-min(s['upgrade_opacity'] for s in samples)>.2,'second upgrade flashes destination panel')
 cmd('shot',name='level-two-task');expected=state();cmd('save');menu();s=resume()
 check(s['upgrade']==1 and '·  将矿镐升至2级' in rows(s),'continue restores pending level-two objective')
 check(s['completed']==expected['completed'] and s['copper']==expected['copper'],'continue restores inventory and objective bits')
 c.key('Tab');time.sleep(2.5);check(state()['completed']&16 and not any('升级' in r or '升至' in r for r in rows(state())),'level two completes and removes upgrade objective')
 cmd('toggle_ui');check(state()['ui_hidden'],'all UI GM switch enables');cmd('shot',name='all-ui-hidden');cmd('toggle_ui');check(not state()['ui_hidden'],'all UI GM switch restores');cmd('shot',name='all-ui-restored')
 pickup_bird()
 cmd('area_fixture',area=3);time.sleep(4)
 check('·  继续前进探索' in rows(state()),'pending prototype objective keeps its identity hidden')
 stage=actors('CardRoomStage')[0];q=stage['position'];cmd('pose',eye=[q[0]-75,q[1],q[2]+115],target=q)
 toast=wait(lambda s:s.get('task_toast_visible') and '发现第3个灵感原型' in s.get('task_toast',''),20)
 check(toast['task_toast'].startswith('✓'),'completed prototype also shows the upper-center checkmark notice');cmd('shot',name='prototype-task-toast')
 cmd('save');s=menu();check(s['has_save'],'return to menu preserves Continue')
 s=start(True);check(s['upgrade']==0 and s['copper']==0 and s['iron']==0 and not s['bomb'] and not s['drill'],'Keep resets upgrades, resources and abilities')
 check(not s['pickaxe'] and not s['jetpack'] and not s['soil_passed'] and s['completed']==0,'Keep resets equipment and tutorial')
 check(s['collection']==1 and s['run_items']==[],'Keep retains catalogue only')
 bird=next(a for a in actors('MiningCollectible') if a.get('item')==3 and a['position'][2]>0);check(not bird['collected'],'previously unlocked pigeon spawns in new game')
 rock=next(a for a in actors('BM_MainGame_StartRoomJunction') if 'cuts' in a);check(rock['cuts']==0,'new game rebuilds terrain')
 cmd('save');menu();s=resume();check(s['collection']==1 and s['run_items']==[],'retained catalogue survives saving and continuing')
 bird=next(a for a in actors('MiningCollectible') if a.get('item')==3 and a['position'][2]>0);check(not bird['collected'],'continue does not remove unlocked but uncollected world item')
 # Upgrade via the real GM action, then re-enter the menu in the same process/GI.
 for _ in range(4):cmd('gm_tool',tool=0)
 cmd('gm_tool',tool=1);cmd('gm_tool',tool=2)
 check(state()['upgrade']==4 and state()['bomb'] and state()['drill'],'actual GM grants max level and abilities')
 menu();s=start(False)
 check(s['upgrade']==0 and not s['bomb'] and not s['drill'],'same-instance New Game clears GM equipment')
 check(s['collection']==0 and not s['run_items'],'Clear removes all catalogue progress')
 check(not s['pickaxe'] and not s['jetpack'] and s['completed']==0,'Clear resets all starter and tutorial state')
 cmd('save');menu();s=resume();check(s['upgrade']==0 and s['collection']==0,'cleared new game is the save continued next time')
 close()
 (out/'results.json').write_text(json.dumps({'passed':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf8')
finally:
 close()
 if not (out/'results.json').exists():
  out.mkdir(parents=True,exist_ok=True);(out/'partial-checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf8')
