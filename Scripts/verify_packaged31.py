"""Main-menu exit confirmation and clockwise menu tilt in the Shipping build."""
import argparse,json,shutil,subprocess,time,hashlib
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');ap.add_argument('save_directory');a=ap.parse_args()
out=Path(a.output).resolve();profile=out/'profile';assert not profile.exists();shutil.copytree(a.save_directory,profile/'Saved/SaveGames')
p=subprocess.Popen([str(Path(a.exe).resolve()),'-windowed','-ResX=1600','-ResY=900','-NoSplash','-MiningQA','-UserDir='+str(profile)])
c=Client(profile);checks=[]
def check(ok,label):
 assert ok,label
 checks.append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
 r=c.command(op,**kw);assert r['ok'],(op,kw,r);return r
def wait(pred,seconds=90):
 end=time.monotonic()+seconds
 while time.monotonic()<end:
  s=c.command('state',timeout=120)
  if pred(s):return s
  time.sleep(.2)
 raise AssertionError('state timeout: '+str(s))
def menu_click(action):
 for phase in ['move','down','up']:cmd('menu_pointer',action=action,phase=phase)
 time.sleep(.4)
def slate_click(tag):
 for phase in ['move','down','up']:cmd('slate_pointer',tag=tag,phase=phase)
 time.sleep(.4)
def saves():return {f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in (profile/'Saved/SaveGames').glob('*.sav')}
try:
 s=c.command('state',timeout=100);s=wait(lambda s:len(s['menu_buttons'])==3)
 cmd('menu_pointer',action='away',phase='move');time.sleep(.7);s=c.command('state');rows=s['menu_buttons']
 check(all(r['angle']>0 for r in rows),'all labels slope from upper left toward lower right')
 check(all(r['angle']==7 and r['shear']==10 for r in rows),'all three buttons stay parallel using the Start angle')
 check(rows[0]['position'][0]>rows[1]['position'][0]>rows[2]['position'][0],'descending rows still stagger from right to left')
 cmd('shot',name='menu-clockwise');initial=saves();menu_click('exit');s=c.command('state')
 check(p.poll() is None and s['opening'] and s['quit_confirmation'],'Exit opens confirmation without terminating game')
 check(set(cmd('slate_ui')['slate_tags'])=={'OpeningQuitCancel','OpeningQuitConfirm'},'modal presents cancel and confirm actions')
 cmd('shot',name='quit-confirmation');slate_click('OpeningQuitCancel');s=c.command('state')
 check(s['opening'] and not s['menu_open'] and not s['quit_confirmation'],'Cancel returns to the main menu')
 check(s['move_ignored'] and s['look_ignored'],'Cancel preserves opening camera and movement locks')
 check(saves()==initial,'cancelled exit does not change saves')
 menu_click('exit');cmd('slate_key',key='Escape');time.sleep(.3)
 check(not c.command('state')['quit_confirmation'] and p.poll() is None,'Escape cancels exit confirmation')
 menu_click('exit');cmd('slate_key',key='Enter');time.sleep(.3)
 check(not c.command('state')['quit_confirmation'] and p.poll() is None,'default keyboard action safely cancels')
 menu_click('continue');s=wait(lambda s:not s['opening']);time.sleep(1)
 check(not s['move_ignored'] and not s['look_ignored'],'Continue after repeated cancellation restores gameplay input')
 # Wait until the loaded checkpoint/terrain settles and the existing save
 # readiness guard allows saving before testing the actual Return action.
 deadline=time.monotonic()+60
 while not c.command('save')['ok']:
  assert time.monotonic()<deadline,'restored terrain did not become ready to save'
  time.sleep(.3)
 cmd('slate_key',key='Escape');s=wait(lambda s:s['menu_open']);tags=cmd('slate_ui')['slate_tags']
 check(set(tags)=={'PauseAction_继续游戏','PauseAction_查看藏品','PauseAction_设置','PauseAction_保存进度','PauseAction_返回主界面'},'Escape menu has only five actions and no quit option')
 cmd('shot',name='pause-without-quit');slate_click('PauseAction_返回主界面');wait(lambda s:s['opening'] and bool(s['menu_buttons']));time.sleep(.7)
 check(c.command('state')['has_save'],'Return to main menu preserves saved progress')
 before=saves();menu_click('exit');cmd('shot',name='quit-final');slate_click('OpeningQuitConfirm');p.wait(timeout=30)
 check(p.returncode==0,'Confirm exits the actual packaged process successfully')
 check(saves()==before,'confirmed main-menu exit preserves saves')
 (out/'results.json').write_text(json.dumps({'passed':True,'checks':checks},ensure_ascii=False,indent=2),encoding='utf8')
finally:
 if p.poll() is None:c.command('quit');p.wait(timeout=60)
