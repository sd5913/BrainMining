"""Isolated stage framing after the complete mining/trigger regression has passed."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardRobot'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True);s=dict(phase=0,next=0)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now<s['next']:return
        if s['phase']==9:
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        if s['phase']==0:helper.start_floating_preview();s.update(phase=1,next=now+3);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            pc=unreal.GameplayStatics.get_player_controller(w,0);p=unreal.GameplayStatics.get_player_pawn(w,0)
            if not p:return
            e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0]
            e.set_actor_tick_enabled(False)
            e.get_editor_property('stage_celebration').set_actor_tick_enabled(True)
            p.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            p.set_actor_location(unreal.Vector(30800,11750,9996),False,True)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(p.get_actor_eyes_view_point()[0],unreal.Vector(31900,11750,10300)))
            s.update(pc=pc,p=p,e=e,phase=2,next=now+.25);return
        if s['phase']==2:
            helper.capture_preview(s['pc'],str(out/'stage-curtain-moving.png'))
            s.update(phase=3,next=now+1);return
        if s['phase']==3:
            assert not s['e'].get_editor_property('stage_celebration').get_editor_property('bStageActive')
            s['p'].set_actor_location(unreal.Vector(31700,11750,9996),False,True)
            s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['p'].get_actor_eyes_view_point()[0],unreal.Vector(32500,11750,10250)))
            s.update(phase=4,next=now+3.8);return
        if s['phase']==4:
            assert s['e'].get_editor_property('stage_celebration').get_editor_property('bStageActive')
            helper.capture_preview(s['pc'],str(out/'stage-confetti-falling.png'))
            (out/'stage-capture.json').write_text(json.dumps(dict(passed=True,scope='Isolated stage camera; full excavation covered by CardP2Flow/visual.json')),encoding='utf-8')
            levels.editor_request_end_play();s.update(phase=9,next=now+2)
    except Exception:
        (out/'stage-capture.json').write_text(json.dumps(dict(passed=False,error=traceback.format_exc())),encoding='utf-8');levels.editor_request_end_play();s.update(phase=9,next=now+2)
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
