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
    s=cmd('state');check(s['prototypes']==0 and 'phase=0' in s['office'],'Office entrance does not award the prototype')
    devices=cmd('actors',filter='PopupOfficeDevice')['actors']
    d=devices[0];p=d['position'];f=d['forward']
    cmd('pose',eye=[p[0]-190*f[0],p[1]-190*f[1],p[2]+170],target=[p[0],p[1],p[2]+124])
    s=wait(lambda s:s['prototypes']==1)
    check(s['subtitle']=='工作是一个能引发共鸣的话题','Prototype shares the workstation narration trigger')
    c.key('E');time.sleep(.5);s=cmd('state')
    check('PopupOfficeDevice' in s['view_target'],'Real E interaction enters the computer')
    check(desktop(s)['body']=='21天gamejam游戏创作主题公布！','Shipping begins with theme announcement');cmd('shot',name='announcement')
    s=wait(lambda s:desktop(s)['body']=='【涌现】',15);check(desktop(s)['handled']==0,'Unattended theme appears without auto-reply');cmd('shot',name='theme')
    s=wait(lambda s:desktop(s)['phase']==0,15);check(desktop(s)['pending']>=3,'Unattended normal popups retain backlog');cmd('shot',name='design')
    for seconds,prefix in [(30,'制作'),(48,'测试'),(60,'提交')]:
        s=wait(lambda s:elapsed(s)>=seconds)
        check(desktop(s)['phase']=={30:1,48:2,60:3}[seconds],'Shipping phase '+prefix)
        check(all(d['accounting_valid'] for d in s['office_desktops']),'Pending accounting '+prefix);cmd('shot',name='phase-'+str(seconds))
        if seconds==30:
            s=wait(lambda s:desktop(s)['body']=='正在重新连接5/5',15)
            check(desktop(s)['phase']==1,'Reconnect interruption occurs during production');cmd('shot',name='reconnecting')
    s=wait(lambda s:'phase=2' in s['office']);cmd('shot',name='burst-transition')
    s=wait(lambda s:'view=0' in s['office'] and float(re.search(r'spray=([0-9.]+)',s['office'])[1])>1)
    check(not s['move_ignored'] and not s['look_ignored'],'Burst restores player controls');cmd('shot',name='burst')
    s=wait(lambda s:float(re.search(r'spray=([0-9.]+)',s['office'])[1])>=17)
    check('paper_below_floor=0' in s['office'],'Eruption keeps paper above the floor');cmd('shot',name='burst-finished')
    report['passed']=True
except Exception:
    report.update(passed=False,error=traceback.format_exc());print(report['error'],flush=True)
finally:
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    if proc and proc.poll() is None:
        try:cmd('quit');proc.wait(timeout=45)
        except Exception:proc.terminate()
if not report['passed']:raise SystemExit(1)
