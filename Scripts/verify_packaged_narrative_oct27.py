"""Opt-in Shipping smoke test, isolated profile; checkpoints are traversal fixtures."""
import argparse,json,re,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('exe');ap.add_argument('output');args=ap.parse_args()
out=Path(args.output).resolve();profile=out/'profile';assert not profile.exists()
out.mkdir(parents=True,exist_ok=True);report={'checks':[]};proc=None

def check(ok,label):
    assert ok,label
    report['checks'].append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
    r=c.command(op,**kw);assert r['ok'],(op,r);return r
def wait(predicate,seconds=100):
    deadline=time.monotonic()+seconds
    while True:
        state=cmd('state')
        if predicate(state):return state
        assert time.monotonic()<deadline,state
        time.sleep(.15)
def desktop(s):return next(d for d in s['office_desktops'] if d['device']==0)
def elapsed(s):return float(re.search(r'elapsed=([0-9.]+)',s['office'])[1])
try:
    proc=subprocess.Popen([str(Path(args.exe).resolve()),'-windowed','-ResX=1280','-ResY=800','-RenderOffscreen','-NoSplash','-MiningQA','-UserDir='+str(profile)])
    c=Client(profile);wait(lambda s:bool(s['menu_buttons']))
    cmd('start');wait(lambda s:not s['opening']);time.sleep(3)
    check(cmd('state')['prototypes']==0,'No first prototype awarded at new-game start')
    cmd('area_fixture',area=1);time.sleep(2)
    s=cmd('state');check(s['prototypes']==1 and 'phase=0' in s['office'],'Office entrance restores the original prototype trigger')
    # The traversal fixture also starts the skipped mining tutorial narration.
    wait(lambda s:not s['subtitle'],30)
    devices=cmd('actors',filter='PopupOfficeDevice')['actors']
    d=devices[0];p=d['position'];f=d['forward']
    cmd('pose',eye=[p[0]-190*f[0],p[1]-190*f[1],p[2]+170],target=[p[0],p[1],p[2]+124])
    s=wait(lambda s:s['prototypes']==1)
    wait(lambda s:s['subtitle']=='工作是一个能引发共鸣的话题')
    cmd('sample_start',label='first-computer-entry')
    c.key('E');time.sleep(1);s=cmd('state')
    report['computer_entry']=cmd('sample_end')
    check(max(report['computer_entry']['frame_ms'])<200,'First computer entry has no frame stall above 200 ms')
    check(report['computer_entry']['max_camera_step_cm']<45,'Computer camera advances smoothly without an instant jump')
    check('PopupOfficeDevice' in s['view_target'],'Real E interaction enters the computer')
    check(desktop(s)['body']=='21天gamejam游戏创作主题公布！','Shipping begins with theme announcement');cmd('shot',name='announcement')
    time.sleep(20);s=cmd('state')
    check(desktop(s)['body']=='21天gamejam游戏创作主题公布！','Normal opening requires a manual theme click')
    s=wait(lambda s:elapsed(s)>=46)
    check(desktop(s)['body']=='【涌现】','Long-idle fallback reveals the theme')
    s=wait(lambda s:elapsed(s)>=56)
    check(desktop(s)['phase']>=0,'Later messages follow the fallback theme')
    check(all(d['accounting_valid'] for d in s['office_desktops']),'Fallback keeps backlog accounting')
    s=wait(lambda s:'phase=2' in s['office']);cmd('shot',name='burst-transition')
    s=wait(lambda s:'view=0' in s['office'] and float(re.search(r'spray=([0-9.]+)',s['office'])[1])>1)
    check(all(int(re.search(r'Source%d emitted=(\d+)'%i,s['office'])[1])>0 for i in (0,1)),'Both computers emit their textured popup models')
    check(not s['move_ignored'] and not s['look_ignored'],'Burst restores player controls');cmd('shot',name='burst')
    s=wait(lambda s:float(re.search(r'spray=([0-9.]+)',s['office'])[1])>=17)
    check('paper_below_floor=0' in s['office'],'Eruption keeps paper above the floor');cmd('shot',name='burst-finished')
    report['passed']=True
except Exception:
    report.update(passed=False,error=traceback.format_exc());print(report['error'],flush=True)
finally:
    if proc and proc.poll() is None:
        try:cmd('quit');proc.wait(timeout=45)
        except Exception:proc.terminate();proc.wait(timeout=15)
    report['exit_code']=proc.returncode if proc else None
    if report.get('passed') and report['exit_code']!=0:
        report.update(passed=False,error='Shipping did not exit cleanly: '+str(report['exit_code']))
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if not report['passed']:raise SystemExit(1)
