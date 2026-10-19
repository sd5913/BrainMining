"""Check the ending's actual shared pawn and cross-map inventory in the credits UI."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/PackagePlaytest'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary;s={'phase':0,'next':0,'start':time.monotonic(),'checks':[]}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def check(name,ok):
    assert ok,name
    s['checks'].append(name)
def shot(name):check(name,helper.capture_preview(s['pc'],str(OUT/(name+'.png'))))
def finish(error=None):
    s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
    (OUT/'credits.json').write_text(json.dumps(dict(passed=not error,error=error,checks=s['checks']),ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('busy') or s.get('done'):return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<300,'Ending UI timeout'
        if now<s['next']:return
        if s['phase']==0:
            assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
            helper.start_floating_preview();s.update(phase=1,next=now+8);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            s['pc']=unreal.GameplayStatics.get_player_controller(w,0)
            if not s['pc'] or not s['pc'].get_controlled_pawn() or unreal.GameplayStatics.get_time_seconds(w)<5:return
            check('Ending uses the exact MainGame pawn',s['pc'].get_controlled_pawn().get_class().get_name()=='BP_SharedMiningCharacter_C')
            shot('ending_shared_pickaxe')
            assert helper.prepare_ending_credits_preview(s['pc'],[0,1,0,0,0,1,0,0,0,1,0,0])
            s.update(phase=2,next=now+2);return
        if s['phase']==2:
            shot('ending_credits')
            s['field']=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            check('Credits retain all three owned collection entries',list(s['field'].inventory)==[0,1,0,0,0,1,0,0,0,1,0,0])
            s['field'].toggle_catalogue();s.update(phase=3,next=now+1);return
        if s['phase']==3:
            check('Credits catalogue opens',s['field'].catalogue_open)
            s['field'].toggle_catalogue();s.update(phase=4,next=now+1);return
        if s['phase']==4:
            check('Catalogue closes and returns to ending',not s['field'].catalogue_open and not unreal.GameplayStatics.is_game_paused(w))
            shot('ending_credits_return');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
