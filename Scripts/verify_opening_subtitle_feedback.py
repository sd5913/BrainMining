"""Capture startup frames, then use real E interactions on all bedroom props."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/TraversalFeedback/Opening';out.mkdir(parents=True,exist_ok=True)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);H=unreal.MiningGUIEditorLibrary
s={'phase':'start','start':time.monotonic(),'next':0,'busy':False,'frames':0};r={'passed':False,'checks':[],'startup':[]}
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):assert ok,label;r['checks'].append(label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(E.get_game_world(),cls)
def sub():
    gi=unreal.GameplayStatics.get_game_instance(E.get_game_world())
    return next(x for x in unreal.ObjectIterator(unreal.SubtitleSubsystem) if x.get_outer()==gi)
def finish(error=None):
    r.update(passed=not error,error=error);(out/'runtime.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8')
    if E.get_game_world():L.editor_request_end_play()
    settings.set_editor_property('bThrottleCPUWhenNotForeground',throttle)
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        now=time.monotonic();p=s['phase'];assert now-s['start']<180,'Timeout at '+p
        if p=='start':
            assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview();s['phase']='startup';return
        w=E.get_game_world()
        if not w:return
        if p=='startup':
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            if not pc or not pc.get_controlled_pawn():return
            openings=actors(unreal.MiningOpening);menu=H.read_preview_object(openings[0],'Menu') if openings else None
            name='startup_%02d'%s['frames'];captured=H.capture_preview(pc,str(out/(name+'.png')))
            if captured:
                r['startup'].append({'frame':name,'time':unreal.GameplayStatics.get_time_seconds(w),'menu':bool(menu),'view':str(pc.get_view_target())});s['frames']+=1
            if s['frames']<12 or not menu:return
            check(pc.get_view_target()!=pc.get_controlled_pawn(),'Startup menu uses seated camera')
            check(pc.is_move_input_ignored(),'Startup locks movement')
            s.update(pc=pc,pawn=pc.get_controlled_pawn(),room=actors(unreal.MiningStartRoom)[0],opening=openings[0])
            button=H.read_preview_object(menu,'StartButton')
            check(H.simulate_preview_button(button,True),'Startup cover releases button hit testing')
            check(H.simulate_preview_button(button,False),'Start button receives real click')
            s.update(phase='aim',index=0,next=now+4);return
        if p=='aim':
            tags=['StartRoomView_Robot','StartRoomView_Computer','StartRoomView_Book'];tag=tags[s['index']]
            actor=next(a for a in actors(unreal.Actor) if a.actor_has_tag(tag));center=actor.get_actor_bounds(False)[0]
            pos=unreal.MathLibrary.transform_location(s['room'].get_actor_transform(),unreal.Vector(*[(-235,0,98),(-200,110,98),(-170,210,98)][s['index']]))
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement();s['pawn'].set_actor_location(pos,False,True)
            eye=s['pawn'].get_actor_eyes_view_point()[0];s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,center));sub().stop_all()
            s.update(phase='interact',tag=tag,next=now+.5);return
        if p=='interact':
            check(str(s['pc'].interaction_hud.interaction_action)=='查看','Shared 查看/E prompt '+s['tag'])
            H.send_preview_key(s['pc'],'E',True);H.send_preview_key(s['pc'],'E',False)
            segment=H.read_preview_object(sub(),'ActiveSegment')
            check(segment and str(segment.segment_id)==s['tag'],'Real E starts subtitle '+s['tag'])
            widget=H.read_preview_object(sub(),'Widget');check(widget and widget.is_in_viewport(),'Subtitle visible '+s['tag'])
            s.update(phase='capture',next=now+.3);return
        if p=='capture':
            check(H.capture_preview(s['pc'],str(out/(s['tag']+'.png'))),'Capture '+s['tag']);s.update(phase='expired',next=now+7);return
        if p=='expired':
            check(H.read_preview_object(sub(),'Widget') is None,'Subtitle finishes and removes widget '+s['tag'])
            s['index']+=1
            if s['index']<3:s['phase']='aim';return
            finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
