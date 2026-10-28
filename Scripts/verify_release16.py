"""Isolated runtime checks and visual evidence for release 1.6."""
import argparse,json,subprocess,time,traceback
from pathlib import Path
from packaged_qa_client import Client
ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');args=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=(root/args.output).resolve();profile=out/'profile'
assert not profile.exists();out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'checks':[]};proc=None
def check(ok,label):
    assert ok,label
    report['checks'].append(label);print('PASS '+label,flush=True)
def cmd(op,**kw):
    r=c.command(op,**kw);assert r['ok'],(op,r);return r
def wait(pred,timeout=90):
    until=time.monotonic()+timeout
    while True:
        s=cmd('state')
        if pred(s):return s
        assert time.monotonic()<until,s
        time.sleep(.1)
def toggle():
    cmd('key',key='LeftControl',down=True);c.key('F9');cmd('key',key='LeftControl',down=False)
try:
    for platform in ('Windows','WindowsEditor'):
        config=profile/'Saved/Config'/platform/'GameUserSettings.ini';config.parent.mkdir(parents=True,exist_ok=True)
        config.write_text('[BrainMining.Localization]\nLanguagePreference=zh-Hans\n',encoding='utf8')
    launch=[args.exe] if args.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game']
    flags=[] if args.exe else ['-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
    proc=subprocess.Popen(launch+flags+['-windowed','-ResX=1280','-ResY=800','-RenderOffscreen','-NoSplash','-nosound','-MiningQA','-UserDir='+str(profile)])
    c=Client(profile);c.command('state',timeout=180);wait(lambda s:s['menu_buttons']);cmd('start');wait(lambda s:not s['opening']);time.sleep(1)
    check(not cmd('state')['ui_hidden'],'UI starts visible')
    c.key('F9');check(not cmd('state')['ui_hidden'],'F9 alone leaves UI unchanged')
    toggle();check(cmd('state')['ui_hidden'],'Ctrl+F9 enters recording mode');cmd('shot',name='recording-mode')
    toggle();check(not cmd('state')['ui_hidden'],'Ctrl+F9 restores UI')
    cmd('slate_key',key='F9',ctrl=True);check(cmd('state')['ui_hidden'],'Slate keyboard shortcut enters recording mode')
    cmd('slate_key',key='F9',ctrl=True);check(not cmd('state')['ui_hidden'],'Slate keyboard shortcut restores UI')
    c.key('C');cmd('catalogue_owned_fixture',item=2);cmd('catalogue_rotate',item=2,yaw=0,pitch=0);time.sleep(2)
    check(cmd('state')['catalogue_preview_visible'],'Chemical collectible preview renders')
    cmd('shot',name='reagents-front')
    cmd('catalogue_rotate',item=2,yaw=180,pitch=0);time.sleep(.5);cmd('shot',name='reagents-back')
    toggle();check(cmd('state')['ui_hidden'],'Recording mode hides collection UI');toggle();check(not cmd('state')['ui_hidden'],'Collection UI can be restored')
    c.key('C');cmd('area_fixture',area=5)
    exhibits=cmd('actors',filter='WenSiMirrorExhibit',parts=True)['actors']
    smoke=next(a for a in exhibits if a['kind']==4);p=smoke['position']
    cmd('pose',eye=[p[0]-200,p[1],p[2]+90],target=[p[0],p[1],p[2]+65])
    cmd('shot',name='smoke-first-view');time.sleep(5);cmd('shot',name='smoke-later')
    check(cmd('state')['museum_aim_kind']==4,'Mature smoke remains selectable')
    stone=next(a for a in exhibits if a['kind']==0);p=stone['position']
    cmd('museum_word_fixture',kind=1,word=1)
    cmd('pose',eye=[p[0]-160,p[1],p[2]+55],target=stone['center']);time.sleep(.5)
    check(cmd('state')['museum_aim_kind']==0,'Stone focus remains selectable');cmd('shot',name='museum-focused')
    cmd('aim',target=[p[0],p[1]+190,p[2]+190]);time.sleep(.3);cmd('shot',name='museum-idle')
    cmd('mouse_gallery_fixture');origin=cmd('actors',filter='WenSiMirrorPuzzle')['actors'][0]['position']
    eye=[origin[0]+2235,origin[1]-350,origin[2]+110];target=[origin[0]+2310,origin[1]-350,origin[2]]
    cmd('pose',eye=eye,target=target)
    actual=cmd('state')['camera']
    cmd('pose',eye=[2*eye[i]-actual[i] for i in range(3)],target=target);cmd('aim',target=target)
    time.sleep(.5);cmd('shot',name='mouse-gallery-floor')
    check(not cmd('actors',filter='RewardPlinth')['actors'],'Central mouse-gallery reward plinth removed')
    report['passed']=True
except Exception:
    report['error']=traceback.format_exc();print(report['error'],flush=True)
finally:
    if proc and proc.poll() is None:
        try:cmd('quit');proc.wait(timeout=45)
        except Exception:proc.terminate();proc.wait(timeout=15)
    report['exit_code']=proc.returncode if proc else None
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if not report['passed']:raise SystemExit(1)
