"""Real ending rail, raw movie presentation and credits transition validation."""
import unreal,time,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/TraversalFeedback/Ending';out.mkdir(parents=True,exist_ok=True)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);H=unreal.MiningGUIEditorLibrary
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':'start','start':time.monotonic(),'next':0,'busy':False};r={'passed':False,'checks':[],'nebula_frames':[]}
def check(ok,name):assert ok,name;r['checks'].append(name)
def capture(name):check(H.capture_preview(s['pc'],str(out/(name+'.png'))),'Capture '+name)
def finish(error=None):
    r.update(passed=not error,error=error)
    (out/'runtime.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
    if E.get_game_world():L.editor_request_end_play()
    settings.set_editor_property('bThrottleCPUWhenNotForeground',throttle)
    unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        now=time.monotonic();p=s['phase'];assert now-s['start']<240,'Timeout in '+p
        if p=='start':
            assert L.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo');H.start_floating_preview();s.update(phase='ready',next=now+7);return
        w=E.get_game_world()
        if not w:return
        if p=='ready':
            directors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EndingEmergence)
            if not directors or H.read_preview_object(directors[0],'ToolLight') is None:return
            d=directors[0];pc=unreal.GameplayStatics.get_player_controller(w,0)
            s.update(d=d,pc=pc);H.resize_preview_viewport(pc,1280,720)
            d.preview_film();d.film_speed=4;s.update(phase='nebula');return
        d=s['d']
        if p=='nebula':
            if d.shot<4:H.write_preview_state(d,'ShotTime',str([5.01,10.01,8.01,11.01][d.shot]));return
            if d.shot<=6:
                i=len(r['nebula_frames']);name='nebula_%03d'%i
                H.capture_preview_video_frame(s['pc'],str(out/(name+'.jpg')),85)
                r['nebula_frames'].append({'frame':name,'shot':d.shot,'time':d.shot_time})
                s['next']=now+.1;return
            check(len(r['nebula_frames'])>12,'Recorded continuous motion through real nebula stages')
            s['phase']='skip_to_video';return
        if p=='skip_to_video':
            if d.shot<9:H.write_preview_state(d,'ShotTime',str([5,10,8,11,35,20,18,12,4.5][d.shot]+.01));return
            if d.shot==9:H.write_preview_state(d,'DrillDistance','10000');return
            if d.shot<13:return
            media=H.read_preview_object(d,'FinaleVideoPlayer')
            if not media or not media.is_playing():return
            s.update(media=media,phase='video_pause',next=now+.5);return
        if p=='video_pause':
            s['media'].pause();s.update(phase='video_clean',next=now+.4);return
        if p=='video_clean':
            capture('movie_clean')
            camera=H.read_preview_object(d,'Camera').get_component_by_class(unreal.CameraComponent)
            pp=camera.get_editor_property('post_process_settings');s['camera']=camera;s['old_pp']=pp.copy()
            pp.set_editor_property('override_scene_color_tint',True);pp.set_editor_property('scene_color_tint',unreal.LinearColor(10,0,0,1))
            pp.set_editor_property('override_vignette_intensity',True);pp.set_editor_property('vignette_intensity',1.)
            camera.set_editor_property('post_process_settings',pp)
            s.update(phase='video_filtered_world',next=now+.3);return
        if p=='video_filtered_world':
            capture('movie_with_scene_filter')
            s['camera'].set_editor_property('post_process_settings',s['old_pp'])
            s['source_size']=s['media'].get_video_track_dimensions(-1,-1).to_tuple();r['source_size']=s['source_size']
            # Seek only in this test fixture, then exercise the actual EOS event.
            s['media'].seek(unreal.MathLibrary.from_seconds(max(0,unreal.MathLibrary.get_total_seconds(s['media'].get_duration())-1.2)))
            s['media'].play();s.update(phase='movie_ending',next=now+.3);return
        if p=='movie_ending':
            capture('movie_before_credits');s.update(phase='credits');return
        if p=='credits':
            if not d.complete:return
            check(s['media'].get_video_track_dimensions(-1,-1).to_tuple()==s['source_size'],'Movie dimensions remain fixed through EOS and credits')
            capture('credits_fade_same_viewport');s.update(phase='credits_ready',next=now+3.2);return
        if p=='credits_ready':
            capture('credits_after_movie');H.send_preview_focused_key(s['pc'],'C',True);H.send_preview_focused_key(s['pc'],'C',False)
            s.update(phase='collection',next=now+.6);return
        if p=='collection':
            check(any(f.catalogue_open for f in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)),'Credits collection opens above movie presentation')
            capture('credits_collection_above_movie');finish();return
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
