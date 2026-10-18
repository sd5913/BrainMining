"""Capture actual timed opponent gestures and the floor toy during a complete four-hand duel."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardRobot'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s=dict(phase=0,start=time.monotonic(),next=0,poses={},checks=[])
def check(value,message):
    if not value:raise AssertionError(message)
    s['checks'].append(message)
def finish(ok,error=''):
    (out/'runtime.json').write_text(json.dumps(dict(passed=ok,error=error,checks=s['checks'],poses=s['poses']),ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();s.update(phase=99,next=time.monotonic()+2)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now<s['next']:return
        if s['phase']==99:
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False);return
        if now-s['start']>150:raise RuntimeError('Robot review timed out')
        if s['phase']==0:helper.start_floating_preview();s.update(phase=1,next=now+3);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            if not pawn:return
            stage=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage)[0]
            robots=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomRobot)
            floor=next(a for a in robots if a.actor_has_tag('CardRoomFloorRobot'))
            opponent=next(a for a in robots if a!=floor)
            check(len(robots)==2,'P2 has a floor robot and an articulated opponent')
            check(stage.get_editor_property('bUseRobotOpponent') and not stage.get_editor_property('rabbit_skeletal_mesh'),'Opponent replaces old toy and rabbit')
            for c in opponent.get_components_by_class(unreal.StaticMeshComponent):check(c.static_mesh is not None,'Robot mesh loaded: '+c.get_name())
            s.update(pc=pc,pawn=pawn,stage=stage,robot=opponent,floor=floor,phase=2,next=now+1)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            eye=floor.get_actor_location()+floor.get_actor_forward_vector()*-3500+unreal.Vector(0,0,1050)
            pawn.set_actor_location(eye,False,True);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],floor.get_actor_location()+unreal.Vector(0,0,1050)));return
        if s['phase']==2:
            helper.capture_preview(s['pc'],str(out/'floor-robot.png'))
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_WALKING)
            unreal.SystemLibrary.execute_console_command(w,'BrainMining.ReviewP2',s['pc']);s.update(phase=3);return
        if s['phase']==3:
            state=str(s['robot'].get_editor_property('expression'))
            if state!=s.get('state'):s.update(state=state,changed=now)
            if state not in s['poses'] and now-s['changed']>.20 and not s['robot'].get_editor_property('hidden'):
                components=s['robot'].get_components_by_class(unreal.SceneComponent)
                s['poses'][state]={c.get_name():list(c.get_editor_property('relative_rotation').to_tuple()) for c in components if c.get_name() in ('HeadJoint','ArmL','ArmR','BrowL','BrowR')}
                helper.capture_preview(s['pc'],str(out/('opponent-'+state+'.png')))
            reward=next(c for c in s['stage'].get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='CollectibleOrb')
            if reward.is_visible():
                check(all(p in s['poses'] for p in ['Arrival','Idle','Hurt','Attack','Sick','Fly']),'Actual four-round duel visits arrival, idle, hurt, attack, sick and fly poses')
                check(s['poses']['Hurt']!=s['poses']['Idle'],'Hit reaction changes mechanical joints')
                check(s['poses']['Attack']!=s['poses']['Idle'],'Counterattack animates arms and brows')
                check(s['robot'].get_editor_property('hidden'),'Opponent disappears at wall impact')
                helper.capture_preview(s['pc'],str(out/'impact-clock-clear.png'));finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
