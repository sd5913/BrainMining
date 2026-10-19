"""Capture the three one-time travel subtitles through actual ending transitions."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/PlaytestPolish/Ending';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
s={'phase':0,'busy':False,'next':0,'start':time.monotonic()};report={'passed':False,'checks':[]}
def check(ok,msg):
 assert ok,msg
 report['checks'].append(msg)
def step(p,delay=.5):s.update(phase=p,next=unreal.GameplayStatics.get_time_seconds(s['w'])+delay)
def cap(n):check(helper.capture_preview(s['pc'],str(out/(n+'.png'))),'Captured '+n)
def finish(ok,error=''):
 report.update(passed=ok,error=error);(out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
 unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
 close={'at':time.monotonic()+3}
 def cleanup(dt):
  if ed.get_game_world() or time.monotonic()<close['at']:return
  unreal.unregister_slate_post_tick_callback(close['handle']);unreal.SystemLibrary.quit_editor()
 close['handle']=unreal.register_slate_post_tick_callback(cleanup)
def tick(dt):
 if s['busy']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<480,'Ending cue review timed out'
  if s['phase']==0:
   levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');helper.start_floating_preview();s['phase']=1;return
  w=ed.get_game_world()
  if not w:return
  s['w']=w
  if unreal.GameplayStatics.get_time_seconds(w)<s['next']:return
  p=s['phase']
  if p==1:
   directors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EndingEmergence)
   if not directors:return
   d=directors[0];pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
   if not pawn or unreal.GameplayStatics.get_time_seconds(w)<4 or not d.gates:return
   s.update(d=d,pc=pc,pawn=pawn)
   cap('01-cell-no-key-strip')
   gate=d.gates[0];gate.mine_sphere(gate.get_actor_location(),2000)
   pawn.set_actor_location(d.get_actor_transform().transform_location(unreal.Vector(1800,0,180)),False,True)
   step(2);return
  d=s['d']
  if p==2:
   check(d.gate_index==1 and d.actor_has_tag('EndingHint_Blood'),'First vessel entry triggers the W subtitle')
   cap('02-blood-W');step(3,4.5);return
  if p==3:
   cap('03-blood-after-cue');d.preview_film();step(4,.25);return
  if p==4:
   check(d.shot==0 and d.actor_has_tag('EndingHint_Street'),'First street entry triggers the S subtitle')
   cap('04-street-S');step(5,4.5);return
  if p==5:
   d.set_editor_property('film_speed',4);step(6,.1);return
  if p==6:
   if d.shot<9:return
   check(d.shot==9 and d.actor_has_tag('EndingHint_O'),'First entry inside O triggers the W subtitle')
   cap('05-O-W');finish(True)
 except Exception:finish(False,traceback.format_exc())
 finally:s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
