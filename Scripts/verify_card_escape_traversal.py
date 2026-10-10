"""Physical escape traversal. One initial placement; all later motion uses the original
character flight/movement, and all excavation uses MiningComponent.TryMine (real trace).
Run in a temporary editor with -ExecutePythonScript=<this file>; no map saves.
"""
import unreal, math, time, json, traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
out=Path(unreal.Paths.project_saved_dir())/'Automation/TVEscapeV17'
out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'phase':0,'index':0,'samples':[],'start':time.monotonic(),'landings':0}
pads=[(13900,6500,350),(15100,7800,350),(16500,9000,600),(17900,10200,350),(19400,11400,350),(21100,11750,600)]

def shot(name):
    unreal.SystemLibrary.execute_console_command(s['w'],'Shot SHOWUI filename="'+str(out/name)+'" -nosuffix')
def done(ok,reason):
    s['passed']=ok;s['reason']=reason
    report={k:v for k,v in s.items() if k not in ['w','pawn','pc','move','e','mine','rocks']}
    (out/'traversal.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('CARD_ESCAPE_TRAVERSAL '+str(ok)+' '+reason)
    levels.editor_request_end_play()
    s["phase"]=9;s["cleanup_at"]=time.monotonic()
def tick(dt):
    try:
        now=time.monotonic()
        if s['phase']==9:
            if now-s['cleanup_at']>2:
                unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            return
        if now-s['start']>220:done(False,'Overall timeout');return
        if s['phase']==0:
            levels.editor_request_begin_play();s['phase']=1;return
        w=ed.get_game_world()
        pawn=unreal.GameplayStatics.get_player_character(w,0) if w else None
        if not pawn:return
        if s['phase']==1:
            es=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)
            if not es:return
            e=es[0];pc=unreal.GameplayStatics.get_player_controller(w,0)
            move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            mine=pawn.get_component_by_class(unreal.MiningComponent)
            s.update(w=w,pawn=pawn,pc=pc,move=move,e=e,mine=mine,rocks=list(e.get_editor_property('computer_rocks')))
            # This test isolates movement; the C++ regression exercises actual reward/impact events.
            pawn.set_actor_location(unreal.Vector(12060,3600,3120),False,True)
            e.call_method('OnRewardClaimed',args=(e.get_editor_property('card_stage').get_editor_property('room_id'),))
            e.retry_escape()
            s['phase']=2;s['began']=now;s['step_start']=now
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],unreal.Vector(*pads[0])))
            return
        p=pawn.get_actor_location();e=s['e'];move=s['move'];pc=s['pc']
        if e.get_editor_property('phase')==unreal.MiningEscapePhase.FAILED:
            shot('traversal-failure.png');done(False,'Hazard failure at '+str(p));return
        if now-s['step_start']>25:
            s['mining_status']=str(s['mine'].get_editor_property('status'));s['stuck_position']=[p.x,p.y,p.z];s['cuts']=[r.get_editor_property('successful_cuts') for r in s['rocks']]
            shot('traversal-stuck.png');done(False,'Waypoint timeout');return
        if s['phase']==5:
            if now-s['done_at']>1:done(s.get('walking_exit_frames',0)>10,'Original movement and mining reached exit; final ramp walked with flight disabled')
            return
        if s['phase']==2:
            target=unreal.Vector(*pads[s['index']]);target.z+=e.get_editor_property('water_z');d=target-p;horizontal=math.hypot(d.x,d.y)
            if horizontal<65 and abs(d.z)<120:
                s['samples'].append({'pad':s['index'],'position':[p.x,p.y,p.z],'elapsed':now-s['began']})
                if s['index']==2:shot('01-rising-water.png')
                if s['index']==4:shot('01-book-%d.png'%s['index'])
                
                # Release lift and touch each book before the next ascent.
                s['phase']=4;move.set_flight_input_held(False);return
        elif s['phase']==4:
            target=unreal.Vector(*pads[s['index']]);target.z+=e.get_editor_property('water_z');d=target-p;horizontal=math.hypot(d.x,d.y)
            move.set_flight_input_held(False)
            if move.is_moving_on_ground():
                s['landings']+=1;s['index']+=1;s['phase']=6 if s['index']==len(pads) else 2;s['step_start']=now;return
            if horizontal>20:pawn.add_movement_input(unreal.Vector(d.x/horizontal,d.y/horizontal,0),min(1,horizontal/150),True)
            return
        elif s['phase']==6:
            # Fly forward and upward together from the last box, keeping the TV in view.
            target=e.get_editor_property('computer_checkpoint_position')+unreal.Vector(0,0,160);d=target-p;horizontal=math.hypot(d.x,d.y)
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],e.get_editor_property('computer_entry')))
            if horizontal<60 and abs(d.z)<120:
                s['phase']=7;move.set_flight_input_held(False);return
        elif s['phase']==7:
            move.set_flight_input_held(False)
            if move.is_moving_on_ground():
                s['tv_landing']=[p.x,p.y,p.z];shot('02-hanging-tv.png')
                s['phase']=3;s['index']=0;s['step_start']=now
            return
        else:
            rocks=s['rocks'];axis=e.get_editor_property('tunnel_end')-e.get_editor_property('computer_entry');axis=axis/axis.length()
            if s['index']<len(rocks):
                rock=rocks[s['index']]
                along=(p-e.get_editor_property('computer_entry')).dot(axis)
                # Keep looking ahead on the slope: stopping at a block face releases lift too early.
                target=e.get_editor_property('computer_entry')+axis*(along+300)
                aim=target+unreal.Vector(0,0,65+[0,-150,100][int((now-s['began'])/.4)%3])
                if along>s.get('best_along',-10000)+12:
                    s['best_along']=along;s['progress_at']=now
                recovering=now-s.get('progress_at',now)>.65
                if recovering:
                    # A human can hold lift over an uneven cut edge and clear the floor below.
                    aim=p+axis*220+unreal.Vector(0,0,[-280,-100,180,320][int(now/.4)%4])
                s['recovering']=recovering
                s['recovery_lift']=recovering and (now-s.get('progress_at',now))%2.4<1.5
                if (p-rock.get_actor_location()).dot(rock.get_actor_forward_vector())>rock.get_editor_property('rock_size').x*.5+10:
                    s['samples'].append({'slab':s['index'],'position':[p.x,p.y,p.z],'cuts':rock.get_editor_property('successful_cuts')})
                    s['index']+=1;s['step_start']=now
                    if s['index']==4:shot('03-tv-solid-tunnel.png')
                    return
            else:
                target=e.get_editor_property('exit');aim=target
                s['walking_exit']=True;s['recovering']=False
                if move.is_moving_on_ground():s['walking_exit_frames']=s.get('walking_exit_frames',0)+1
                if e.get_editor_property('phase')==unreal.MiningEscapePhase.ESCAPED:
                    shot('04-safe-exit.png');s['duration']=now-s['began'];s['phase']=5;s['done_at']=now;return
            if s['phase']==5:
                if now-s['done_at']>1:done(s.get('walking_exit_frames',0)>10,'Original movement and mining reached exit; final ramp walked with flight disabled');return
            pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],aim))
            s['mine'].try_mine();d=target-p;horizontal=math.hypot(d.x,d.y)
        move.set_flight_input_held(not s.get('walking_exit',False) and ((s.get('recovery_lift',False) if s['phase']==3 and s.get('recovering',False) else d.z>15)))
        if horizontal>10 and (s['phase']!=2 or d.z<160):pawn.add_movement_input(unreal.Vector(d.x/horizontal,d.y/horizontal,0),min(1,horizontal/180),True)
        if s['phase']==5:
            if now-s['done_at']>1:done(s.get('walking_exit_frames',0)>10,'Original movement and mining reached exit; final ramp walked with flight disabled')
            return
        if s['phase']==2:pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target))
    except Exception:done(False,traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)

