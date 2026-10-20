"""Record the production brain -> street -> upward pullback -> Earth flow in PIE.

Only the initial fixture skips to the final playable neuron. A real Space input
finishes it; every subsequent camera pose comes from the unmodified director.
JPEG frame timestamps and the live master submix preserve real playback timing.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Review/EndingStreet-20261020'/datetime.now().strftime('take-%H%M%S')
FRAMES=OUT/'frames'
FRAMES.mkdir(parents=True,exist_ok=True)
(ROOT/'Saved/Review/EndingStreet-20261020/latest-take.txt').write_text(str(OUT),encoding='utf8')
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
s={'phase':0,'started':time.monotonic(),'next':0,'busy':False,'frames':[],'stages':[],
   'audio_started':False,'done':False}
report={'passed':False,'directory':str(OUT),'camera':'production director only',
        'fixture':'final playable neuron; real Space input triggers the original ending',
        'film_speed':1.0,'target_capture_fps':30}

def finish(error=None):
    if s['done']:return
    s['done']=True
    if s.get('pressed'):
        helper.send_preview_key(s['pc'],'SpaceBar',False)
    if s['audio_started']:
        try:
            unreal.AudioMixerLibrary.stop_recording_output(s['world'],
                unreal.AudioRecordingExportType.WAV_FILE,'live-audio',str(OUT))
        except Exception as exc: report['audio_error']=str(exc)
    report.update(passed=not error,error=error,frames=s['frames'],stages=s['stages'])
    if s['frames']:
        report['duration_seconds']=time.monotonic()-s['record_started']
        for shot in (0,1):
            z=[f['camera'][2] for f in s['frames'] if f['shot']==shot]
            report['street_shot_'+str(shot)]={'frames':len(z),'start_z':z[0] if z else None,
                'end_z':z[-1] if z else None,'max_downward_step':max([a-b for a,b in zip(z,z[1:])]+[0])}
    (OUT/'recording.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+10}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle'])
        unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
    unreal.log('ENDING_STREET_VIDEO_FINISHED '+str(OUT))

def frame():
    started=time.monotonic()
    if started<s.get('next_frame',0):return
    d=s['director']
    path=FRAMES/('frame-%06d.jpg'%len(s['frames']))
    assert helper.capture_preview_video_frame(s['pc'],str(path),90),'Viewport frame capture failed'
    now=time.monotonic()
    camera=s['pc'].get_view_target()
    s['frames'].append({'file':path.name,'seconds':(started+now)*.5-s['record_started'],
        'world_seconds':unreal.GameplayStatics.get_time_seconds(s['world']),
        'shot':d.shot,'gate':d.gate_index,'camera':list(camera.get_actor_location().to_tuple()),
        'pitch':camera.get_actor_rotation().pitch})
    stage=(d.shot,d.gate_index)
    if stage!=s.get('last_stage'):
        s['stages'].append({'shot':stage[0],'gate':stage[1],'seconds':s['frames'][-1]['seconds']})
        s['last_stage']=stage
        unreal.log('ENDING_VIDEO_STAGE '+str(s['stages'][-1]))
    s['next_frame']=started+1/30

def tick(dt):
    if s['busy'] or s['done']:return
    s['busy']=True
    try:
        now=time.monotonic()
        assert now-s['started']<300,'Recording timed out'
        if now<s['next']:return
        if s['phase']==0:
            assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
            helper.start_floating_preview()
            s.update(phase=1,next=now+6)
            return
        world=ed.get_game_world()
        if not world:return
        if s['phase']==1:
            directors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EndingEmergence)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            if not directors or not pc or not pawn or unreal.GameplayStatics.get_time_seconds(world)<5:return
            d=directors[0]
            if not d.gates:return
            goal=len(d.neuron_positions)-1
            host=min(range(goal-5,goal),key=lambda i:(d.neuron_world_position(i)-d.neuron_world_position(goal)).length())
            assert helper.write_preview_state(d,'GateIndex','2')
            assert helper.write_preview_state(d,'CurrentNeuron',str(host))
            assert d.can_possess(goal),'Final neuron must be reachable through the normal input path'
            camera=helper.read_preview_object(d,'Camera')
            assert isinstance(camera,unreal.CameraActor)
            eye=d.neuron_world_position(host)+d.get_actor_up_vector()*380
            camera.set_actor_location(eye,False,False)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,d.neuron_world_position(goal)))
            pc.set_view_target_with_blend(camera,0)
            pawn.set_actor_hidden_in_game(True)
            movement=pawn.get_component_by_class(unreal.CharacterMovementComponent)
            movement.stop_movement_immediately()
            movement.disable_movement()
            pc.set_ignore_move_input(True)
            pc.reset_ignore_look_input()
            d.set_editor_property('film_speed',1.0)
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            s.update(phase=2,world=world,pc=pc,director=d,goal=goal,next=now+3)
            return
        if s['phase']==2:
            s['record_started']=now
            if '-nosound' not in unreal.SystemLibrary.get_command_line().lower():
                try:
                    unreal.AudioMixerLibrary.start_recording_output(world,60)
                    s['audio_started']=True
                except Exception as exc:report['audio_error']=str(exc)
            s.update(phase=3,jump_at=now+2)
        frame()
        if s['phase']==3 and now>=s['jump_at']:
            helper.send_preview_key(s['pc'],'SpaceBar',True)
            s.update(phase=4,pressed=True,release_at=now+.12)
        elif s['phase']==4 and now>=s['release_at']:
            helper.send_preview_key(s['pc'],'SpaceBar',False)
            s.update(phase=5,pressed=False)
        d=s['director']
        if s['phase']==5:
            assert now-s['jump_at']<12 or d.gate_index>=5 or d.shot>=0,'Last neuron jump did not trigger'
            if d.shot>=3:
                if 'earth_at' not in s:s['earth_at']=now
                elif now-s['earth_at']>=2:
                    shots={f['shot'] for f in s['frames']}
                    assert {-1,0,1,2,3}.issubset(shots),'Recording must include both street boundaries'
                    finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
