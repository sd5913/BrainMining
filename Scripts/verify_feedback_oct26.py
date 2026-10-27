"""Opt-in isolated runtime regression checks for the six October 26 fixes."""
import argparse, json, math, subprocess, time, traceback
from pathlib import Path
from packaged_qa_client import Client

ap=argparse.ArgumentParser();ap.add_argument('output');ap.add_argument('--exe');ap.add_argument('--ending',action='store_true');a=ap.parse_args()
root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve();out.mkdir(parents=True)
profile=out/'profile';profile.mkdir()
config=profile/'Saved/Config/Windows';config.mkdir(parents=True)
(config/'Engine.ini').write_text('[Audio]\nUnfocusedVolumeMultiplier=1.0\n')
launch=[str(Path(a.exe).resolve())] if a.exe else ['C:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe',str(root/'BrainMining.uproject'),'/Game/MainGame/Maps/Lvl_MainGame','-game','-NoLiveCoding','-EnablePlugins=GameFeatures','-DisablePlugins=AllToolsets,ModelContextProtocol']
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
proc=subprocess.Popen(launch+['-RenderOffscreen','-windowed','-ResX=1280','-ResY=870','-MiningQA','-UserDir='+str(profile),'-abslog='+str(out/'game.log')],startupinfo=si)
c=Client(profile);r={'checks':[],'passed':False}
def cmd(op,**kw):
    s=c.command(op,timeout=180,**kw);assert s['ok'],(op,kw);return s
def check(ok,label):
    r['checks'].append(dict(passed=bool(ok),name=label));assert ok,label
    print('PASS',label,flush=True)
def actors(name,parts=False):return cmd('actors',filter=name,parts=parts)['actors']
def plus(a,b):return [x+y for x,y in zip(a,b)]
def shot(name):cmd('shot',name=name);time.sleep(.5)
def bird():return next(x for x in actors('MiningCollectible') if x.get('item')==3 and x['position'][2]>0)
def water():return next(x for x in actors('HeavenToiletWater') if x['position'][2]>0)
try:
    cmd('state');cmd('start')
    for _ in range(120):
        if not cmd('state')['opening']:break
        time.sleep(.2)
    cmd('gm_jump',area=2)
    obj=bird();initial=obj['position']
    spec={'approach':[initial[i]-obj['forward'][i]*390 for i in range(3)],'clue':[initial[i]-obj['forward'][i]*64 for i in range(3)]}
    check(not obj['released'],'Resident pigeon starts buried')
    cmd('pose',eye=spec['approach'],target=spec['clue']);time.sleep(.8);shot('pigeon-surface')
    # A side excavation must expose the resident without popping it toward the player.
    eye=plus(initial,[300,350,80]);cmd('pose',eye=eye,target=initial)
    cmd('mine',actor=obj['host'],point=initial,radius=230);time.sleep(.7)
    after=bird();check(after['released'] and math.dist(initial,after['position'])<.1,'Excavated pigeon remains inside its original soil region')
    toward=[eye[i]-after['position'][i] for i in range(3)];length=math.sqrt(sum(x*x for x in toward))
    check(sum(-after['forward'][i]*toward[i]/length for i in range(3))>.97,'Pigeon faces the side excavator')
    cmd('pose',eye=spec['approach'],target=initial);time.sleep(.3);shot('pigeon-exposed')
    # Room audio ownership is tested on entry, exit, and re-entry.
    cmd('gm_jump',area=4);w=water();center=w['position']
    eye=plus(center,[200,0,160]);cmd('pose',eye=eye,target=plus(eye,[100,0,0]));time.sleep(.3)
    check(water()['choir_playing'],'Heaven choir plays on entry')
    eye=plus(center,[1900,0,160]);cmd('pose',eye=eye,target=center);time.sleep(.1)
    check(not water()['choir_playing'],'Heaven choir stops immediately on exit')
    eye=plus(center,[200,0,160]);cmd('pose',eye=eye,target=plus(eye,[100,0,0]));time.sleep(.2)
    check(water()['choir_playing'],'Heaven choir can play again on re-entry')
    # Detect newly launched ducks by their discontinuous return to the nozzle.
    def ducks():
        t=next(x for x in actors('HeavenToilet',True) if x['class']=='HeavenToilet' and x['position'][2]>0)
        return {p['name']:p for p in t['parts'] if 'RubberDuck' in p['mesh']}
    previous=ducks();events=[];started=time.monotonic()
    while time.monotonic()-started<10:
        current=ducks();now=time.monotonic()-started
        for name,p in current.items():
            old=previous[name]
            if p['visible'] and (not old['visible'] or math.dist(p['position'],old['position'])>250):events.append((now,name,p['position']))
        previous=current;time.sleep(.04)
    groups=[]
    for e in events:
        if not groups or e[0]-groups[-1][-1][0]>.6:groups.append([])
        groups[-1].append(e)
    complete=[g for g in groups if g[0][0]>.6 and g[-1][0]<9.3]
    r['duck_bursts']=complete
    check(len(complete)>=2 and all(5<=len(g)<=7 for g in complete),'Complete duck bursts contain 5–7 staggered launches')
    t=next(x for x in actors('HeavenToilet') if x['class']=='HeavenToilet' and x['position'][2]>0)
    cmd('pose',eye=plus(t['position'],[500,-350,200]),target=plus(t['position'],[0,0,100]));shot('toilet-ducks')
    cmd('area_fixture',area=5);time.sleep(.5)
    cover=next(x for x in actors('WenSiMirrorExhibit',True) if x.get('kind')==7)
    glass=[p for p in cover['parts'] if p['visible'] and 'DisplayGlass' in p.get('material','')]
    check(len(glass)==4,'Heavy cover has four glazed windows')
    cp=cover['position']
    for n,offset in [('front',[-180,-180,0]),('back',[180,-180,0])]:
        cmd('pose',eye=plus(cp,offset),target=cp);shot('cover-'+n)
    # The original front opening stays clear; all three new lower panels stay solid.
    hole=plus(cp,[-94,0,-85]);hit=cmd('trace',**{'from':plus(hole,[-60,0,0]),'to':plus(hole,[60,0,0])})
    check(hit.get('hit_actor','')!=cover['name'],'Original small entrance is retained')
    for direction in ([1,0,0],[0,1,0],[0,-1,0]):
        point=plus(cp,[direction[0]*94,direction[1]*94,-85])
        hit=cmd('trace',**{'from':plus(point,[x*60 for x in direction]),'to':plus(point,[-x*60 for x in direction])})
        check(hit.get('hit_actor','')==cover['name'],'Additional window retains a solid lower wall '+str(direction))
    cmd('mouse_gallery_fixture');portal=actors('MouseMuseumPortal',True)[0];cell=portal['cell']
    cmd('pose',eye=plus(cell,[-90,0,0]),target=plus(cell,[0,6,0]));time.sleep(.3)
    cmd('aim',target=plus(cell,[0,6,0]));time.sleep(.3)
    portal=actors('MouseMuseumPortal',True)[0]
    check(portal['focused'],'Painting circle is selectable six centimeters off-center')
    cmd('pose',eye=plus(cell,[-90,0,0]),target=plus(cell,[0,-30,0]));time.sleep(.3)
    cmd('aim',target=plus(cell,[0,-30,0]));time.sleep(.2)
    def highlight():return next(p['highlight'] for p in actors('MouseMuseumPortal',True)[0]['parts'] if 'highlight' in p)
    h1=highlight();time.sleep(.65);h2=highlight()
    check(abs(h1-h2)>.03,'Painting circle pulses without being aimed at');shot('painting-circle')
    if a.ending:
        cmd('aim',target=cell);time.sleep(.2)
        check(actors('MouseMuseumPortal')[0]['focused'],'Actual painting remains focused before entering')
        c.key('E');deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            state=cmd('state')
            if 'EndingEmergence' in state.get('map',''):break
            time.sleep(.3)
        check('EndingEmergence' in state.get('map',''),'Painting interaction still travels to the ending')
    r['passed']=True
except:
    r['error']=traceback.format_exc();print(r['error'],flush=True)
finally:
    (out/'results.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    try:cmd('quit');proc.wait(timeout=30)
    except:proc.terminate()
if not r['passed']:raise SystemExit(1)

