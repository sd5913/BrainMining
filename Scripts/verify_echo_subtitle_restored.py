"""Verify the restored one-shot landing narration in MainGame's PIE copy."""
import json,time,traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/EchoSubtitleRecovery';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not ed.get_game_world()
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
report={'passed':False,'checks':[]}
state={'phase':'wait','start':time.monotonic(),'at':0}
H=unreal.MiningGUIEditorLibrary

def check(ok,label):
    assert ok,label
    report['checks'].append(label)

def finish(error=None):
    report['error']=error;report['passed']=error is None
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();state.update(phase='cleanup',at=time.monotonic())

def tick(dt):
    try:
        now=time.monotonic()
        if state['phase']=='cleanup':
            if not ed.get_game_world() and now-state['at']>1:
                unreal.unregister_slate_post_tick_callback(handle)
                unreal.log('ECHO_SUBTITLE_RESTORED_VERIFIED '+str(report['passed']))
            return
        assert now-state['start']<100,'Echo subtitle validation timed out'
        w=ed.get_game_world()
        if not w:return
        if state['phase']=='wait':
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            ds=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MainGameSubtitleDirector)
            es=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EchoPassageStage)
            os=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening)
            if not pc or not ds or not es or not os:return
            state.update(pc=pc,d=ds[0],e=es[0],o=os[0],w=w)
            if H.read_preview_object(os[0],'Menu') is None:return
            os[0].start_game();state.update(phase='activate',at=now);return
        if state['phase']=='activate':
            if not state['o'].get_editor_property('bFinished'):return
            check(H.prepare_collection_preview_area(state['pc'],3,False),'Activate EchoMaze area in MainGame')
            pawn=state['pc'].get_controlled_pawn()
            move=pawn.get_component_by_class(unreal.CharacterMovementComponent)
            move.stop_movement_immediately();move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            state.update(pawn=pawn,move=move,phase='before',at=now)
            gi=unreal.GameplayStatics.get_game_instance(w)
            state['sub']=next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==gi)
            state['sub'].stop_all();return
        if now-state['at']<.35:return
        if state['phase']=='before':
            check(not state['d'].has_triggered('盲人敲地玩法'),'No narration before landing')
            check('Lvl_MainGame_SubtitleTriggers' in state['d'].get_path_name(),'Director belongs to MainGame subtitle sublevel')
            expected=unreal.load_asset('/Game/Subtitles/DA_EchoMaze')
            state['texts']=[str(l.text) for l in expected.lines]
            report['lines']=state['texts']
            state['sub'].stop_all()
            state['e'].emit_echo(state['e'].get_actor_location(),True)
            state.update(phase='landed',at=now)
        elif state['phase']=='landed':
            check(state['d'].has_triggered('盲人敲地玩法'),'Landing triggers EchoMaze narration')
            active=H.read_preview_object(state['sub'],'ActiveSegment')
            check(active is not None and str(active.segment_id)=='盲人敲地玩法','Correct subtitle is actively playing')
            check([str(l.text) for l in active.lines]==state['texts'],'Preserve latest authored EchoMaze text')
            check(all(l.audio is None for l in active.lines),'Procedural voice without audio file')
            check(any(s.is_in_viewport() and s.is_visible() for s in unreal.ObjectIterator(unreal.SubtitleWidget)),'Shared subtitle widget is visible')
            check(H.capture_preview(state['pc'],str(out/'landing-subtitle.png')),'Capture landing subtitle')
            state['sub'].stop_all()
            state['e'].emit_echo(state['e'].get_actor_location(),True)
            state.update(phase='repeat',at=now)
        elif state['phase']=='repeat':
            check(H.read_preview_object(state['sub'],'ActiveSegment') is None,'Repeated landing does not replay narration')
            finish()
    except Exception:finish(traceback.format_exc())

assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
