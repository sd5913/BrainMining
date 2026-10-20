"""Inspect the editable UMG credits after their real fade timings."""
import unreal,json,time,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback16';H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};report={'passed':False,'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):
 assert ok,label
 report['checks'].append(label)
def run():
 assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');H.start_floating_preview()
 while not ed.get_game_world():yield .2
 w=ed.get_game_world()
 while not unreal.GameplayStatics.get_player_pawn(w,0):yield .2
 pc=unreal.GameplayStatics.get_player_controller(w,0);yield 4
 H.resize_preview_viewport(pc,1280,720)
 check(H.prepare_ending_credits_preview(pc,[1]*12),'Activate shared ending completion and collection inventory')
 yield 4
 d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EndingEmergence)[0];ui=H.read_preview_object(d,'CreditsWidget')
 check(ui is not None and 'WBP_EndingCredits_C' in ui.get_class().get_name(),'Runtime uses the saved editable Widget Blueprint')
 bg=H.read_preview_object(ui,'BlackBackground');group=H.read_preview_object(ui,'TextGroup');prompt=H.read_preview_object(ui,'Prompt')
 check(bg.get_render_opacity()>.99 and group.get_render_opacity()>.99,'Background and text finish their editable fade timings')
 check(prompt.get_render_opacity()==0,'Exit hint respects the eight second delay')
 check(H.capture_preview(pc,str(OUT/'07_editable_credits.png')),'Capture mature credits')
 check(abs(ui.get_editor_property('crt_strength')-.055)<.001,'CRT effect remains restrained and editable')
 yield 5
 check(prompt.get_render_opacity()>.1,'Delayed exit hint appears')
 check(H.capture_preview(pc,str(OUT/'08_credits_prompt.png')),'Capture credits controls')
 H.send_preview_key(pc,'C',True);H.send_preview_key(pc,'C',False);yield .4
 field=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField) if a.catalogue_open)
 check(field is not None,'C still opens collection over the credits')
 H.send_preview_key(pc,'Escape',True);H.send_preview_key(pc,'Escape',False);yield .3
 check(not field.catalogue_open,'Escape closes catalogue and returns to credits')
 report['passed']=True

gen=run()
def tick(dt):
 if s.get('busy') or time.monotonic()<s['next']:return
 s['busy']=True
 try:
  assert time.monotonic()-s['start']<160,'Timeout'
  s['next']=time.monotonic()+next(gen)
 except StopIteration:finish()
 except Exception:finish(traceback.format_exc())
 finally:s['busy']=False
def finish(error=None):
 if error:report.update(passed=False,error=error);unreal.log_error(error)
 (OUT/'credits_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 perf.set_editor_property('bThrottleCPUWhenNotForeground',old);unreal.unregister_slate_post_tick_callback(handle)
 if ed.get_game_world():levels.editor_request_end_play()
 unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
