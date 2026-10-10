"""PIE smoke check: drive the unchanged pawn through V07 waypoints using movement input only.
Launch editor with -ExecutePythonScript=<this file>, the script starts a temporary PIE session automatically.
The only teleport is initial placement at the foot of the stairs. No jump or flight inputs.
"""
import unreal, json, time, traceback
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'Automation/StudyV07'
route=json.loads((out/'layout.json').read_text(encoding='utf-8'))['route']
state={'started':False,'index':1,'max_z':0,'flying_frames':0,'samples':[]}
def finish(ok,reason):
    state['passed']=ok;state['reason']=reason
    (out/'walk.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
    unreal.log('STUDY_V07_WALK '+str(ok)+' '+reason)
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(dt):
    try:
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:return
        pawn=unreal.GameplayStatics.get_player_character(world,0)
        if not pawn:return
        movement=pawn.get_component_by_class(unreal.CharacterMovementComponent)
        if not state['started']:
            pawn.set_actor_location(unreal.Vector(9900,1580,130),False,True)
            movement.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
            state['started']=True;state['began']=time.monotonic();state['step_started']=time.monotonic()
            state['walkable_floor_angle']=movement.get_walkable_floor_angle();state['max_step_height']=movement.get_editor_property('max_step_height')
            return
        pos=pawn.get_actor_location();state['max_z']=max(state['max_z'],pos.z)
        if movement.is_flying():state['flying_frames']+=1
        target=route[state['index']];dx=target[0]-pos.x;dy=target[1]-pos.y;dist=math.hypot(dx,dy)
        if dist<45:
            state['samples'].append({'index':state['index'],'position':[pos.x,pos.y,pos.z]})
            state['index']+=1;state['step_started']=time.monotonic()
            if state['index']>=len(route):
                finish(pos.z>3050 and state['flying_frames']==0,'Reached tabletop with walking input');return
            return
        if time.monotonic()-state['step_started']>12:
            state['stuck_position']=[pos.x,pos.y,pos.z];state['target']=target;finish(False,'Waypoint timeout');return
        pawn.add_movement_input(unreal.Vector(dx/dist,dy/dist,0),min(1,dist/140),True)
        controller=unreal.GameplayStatics.get_player_controller(world,0)
        controller.set_control_rotation(unreal.Rotator(pitch=5,yaw=math.degrees(math.atan2(dy,dx))))
    except Exception:
        finish(False,traceback.format_exc())
import math
handle=unreal.register_slate_post_tick_callback(tick)
unreal.log('STUDY_V07_WALK_ARMED')


unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()


