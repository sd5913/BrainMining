"""Polish32 checkpoint regression with actual input, bomb and card interactions."""
import argparse,json,subprocess,time
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');a=ap.parse_args()
out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists()
exe=str(Path(a.exe).resolve());checks=[];p=None
def launch():
 global p,c
 p=subprocess.Popen([exe,'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
 c=Client(profile);c.command('state',timeout=180);return wait(lambda s:s['opening'] and bool(s['menu_buttons']))
def close():
 if p and p.poll() is None:c.command('quit');p.wait(timeout=60)
def cmd(op,**kw):
 r=c.command(op,**kw);assert r['ok'],(op,kw,r);return r
def check(ok,label):
 assert ok,label
 checks.append(label);print('PASS '+label,flush=True)
def wait(pred,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=c.command('state')
  if pred(s):return s
  time.sleep(.2)
 raise AssertionError('state timeout '+str(s))
def actors(f):return cmd('actors',filter=f)['actors']
def area(i):cmd('area_fixture',area=i);time.sleep(2)
try:
 s=launch();check(len(actors('BM_Polish32_BombSign'))==12,'all six walls have authored stencils before New Game or looking at them')
 cmd('start');wait(lambda s:not s['opening'])
 for pickup in actors('StarterEquipment'):
  cmd('pose',actor=pickup['path'],offset=[-125,0,50]);time.sleep(.3);cmd('interact')
 cmd('gm_tool',tool=0);cmd('gm_tool',tool=1);cmd('gm_tool',tool=2)
 check(c.command('state')['drill_capacity']==60,'drill total capacity is sixty percent of 100')
 area(2)
 placements=json.loads((Path(__file__).resolve().parent/'polish32_scene_checkpoints.json').read_text(encoding='utf8'))
 for name in ['MAIN_GAME_OfficeExit_BombOnly','MAIN_GAME_BombOnly_2_7']:
  sign=next(r for r in placements['signs'] if r['wall']==name);q=sign['position'];n=sign['normal'];eye=[q[i]+n[i]*350 for i in range(3)]
  # Office wall moves once at runtime; use its attached decal's actual location.
  if 'OfficeExit' in name:
   sign_actor=min(actors('BM_Polish32_BombSign'),key=lambda r:r['position'][0])
   q=sign_actor['position'];eye=[q[0]-350,q[1],q[2]]
  cmd('pose',eye=eye,target=q);time.sleep(.5);cmd('shot',name='bomb-'+name)
 c.key('V');time.sleep(1);cmd('shot',name='drill-gameplay');c.key('V')
 area(5)
 reset=actors('WenSiResetButton')[0];cmd('pose',eye=[reset['position'][0]-130,reset['position'][1],reset['position'][2]+100],target=[reset['position'][0],reset['position'][1],reset['position'][2]+70]);s=wait(lambda s:s['aim_reset'],10)
 before=s['reset_count'];c.key('E');time.sleep(.3);check(c.command('state')['reset_count']==before,'tap E does not reset museum')
 cmd('key',key='E',down=True);time.sleep(1.2);s=c.command('state');check(.3<s['reset_hold']<.65 and s['reset_count']==before,'hold progress fills before three seconds');cmd('shot',name='museum-hold-progress');cmd('key',key='E',down=False);time.sleep(.3)
 check(c.command('state')['reset_hold']==0,'releasing early cancels reset progress')
 cmd('key',key='E',down=True);time.sleep(3.3);s=c.command('state');check(s['reset_count']==before+1,'three-second hold resets exactly once');time.sleep(.7);check(c.command('state')['reset_count']==before+1,'continued hold cannot repeat reset');cmd('key',key='E',down=False)
 # Four actual card selections and resolves, including a long poison idle.
 area(3);stage=actors('CardRoomStage')[0];q=stage['position'];cmd('pose',eye=[q[0]-75,q[1],q[2]+115],target=q);time.sleep(2)
 cmd('card',action='interact');time.sleep(.5);cmd('card',action='interact');time.sleep(5)
 for round in range(1,5):
  s=c.command('state');check(s['card_round']==round and s['card_phase']==4,'card round ready '+str(round));cmd('card',action='all');cmd('card',action='confirm');time.sleep(5.2)
  if round==2:
   s=c.command('state');check(s['poison_paused'],'poison holds its mature cloud');cmd('shot',name='poison-mature')
   sample=c.sample('poison-sustain-20s',20);check(c.command('state')['poison_paused'],'poison remains mature after twenty seconds');cmd('shot',name='poison-still-mature')
 check(c.command('state')['card_phase'] in [6,7],'all four card rounds still reach victory')
 cmd('card',action='interact')
 area(4);toilet=next(r for r in actors('MiningCollectible') if r.get('item')==0 and r['position'][2]>0);q=toilet['position']
 cmd('pose',eye=[q[0],q[1]-160,q[2]+80],target=[q[0],q[1],q[2]+35]);time.sleep(1)
 check(c.command('state')['toilet_outline'],'actual heaven toilet mesh has collection outline');cmd('shot',name='heaven-toilet-outline');cmd('collect');s=wait(lambda s:'有点味道的想法' in s['subtitle'],30);check('有点味道的想法' in s['subtitle'],'toilet pickup subtitle uses new wording')
 c.key('C');time.sleep(.5);cmd('catalogue_rotate',yaw=0,pitch=0);cmd('shot',name='toilet-inspection-front');cmd('catalogue_rotate',yaw=90,pitch=35);time.sleep(.5);cmd('shot',name='toilet-inspection-rotated');c.key('C')
 cmd('save');cmd('gm_map',index=5);wait(lambda s:s.get('ending_gate')==0);time.sleep(2)
 gate=next(r for r in actors('DiggableRock') if '/EndingEmergence/' in r['path']);q=gate['position'];cmd('pose',eye=[q[0]-310,q[1]+120,q[2]+60],target=[q[0]+400,q[1]+120,q[2]+60]);time.sleep(1)
 cmd('sample_start',label='cell-vessel-continuity');cmd('key',key='W',down=True)
 wait(lambda s:s.get('ending_gate')==1,15);cmd('key',key='W',down=False);time.sleep(2)
 sample=cmd('sample_end');(out/'cell-camera.json').write_text(json.dumps(sample,ensure_ascii=False),encoding='utf8');check(sample['max_camera_step_cm']<90,'off-center cell-to-vessel camera has no teleport frame')
 cmd('credits_fixture');time.sleep(2);s=c.command('state');check(not s['has_save'],'credits mark campaign complete');cmd('slate_key',key='Escape');wait(lambda s:s['menu_open']);cmd('return_menu');s=wait(lambda s:s['opening'] and bool(s['menu_buttons']));check([r['text'] for r in s['menu_buttons']]==['开始游戏','退出游戏'],'main menu after credits has no Continue')
 close();s=launch();check(not s['has_save'] and len(s['menu_buttons'])==2,'completed status survives process restart');cmd('start');wait(lambda s:s['collection_choice']);cmd('confirm_new',keep=True);s=wait(lambda s:not s['opening']);check(s['collection']>0 and s['upgrade']==0 and s['run_items']==[],'new run after credits can retain catalogue but resets gameplay')
 (out/'results.json').write_text(json.dumps(dict(passed=True,checks=checks),ensure_ascii=False,indent=2),encoding='utf8')
finally:
 close()
 if not (out/'results.json').exists():out.mkdir(parents=True,exist_ok=True);(out/'partial.json').write_text(json.dumps(checks,ensure_ascii=False),encoding='utf8')
