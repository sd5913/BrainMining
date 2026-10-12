"""Capture final cave seams and first-mine fragment palettes in an existing PIE session."""
import unreal,time,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/CavePresentationPreview';out.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);s=dict(step=0,next=0);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def tick(dt):
    try:
        w=editor.get_game_world()
        if not w or time.perf_counter()<s['next']:return
        pawn=unreal.GameplayStatics.get_player_character(w,0);pc=unreal.GameplayStatics.get_player_controller(w,0)
        if not pawn:return
        if s['step']==0:
            hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
            points=[]
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.GeneratedMiningRock):
                if a.get_actor_label().startswith('Campaign_TunnelGround') and a.actor_has_tag('BM_Campaign_Mine01'):
                    points.append((a.get_actor_location(),a.rock_size.x))
            points.sort(key=lambda v:v[0].x);s['points']=points
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
        i=s['step']//2
        if i>=len(s['points']):
            unreal.unregister_slate_post_tick_callback(handle);unreal.log('CAVE_PREVIEW_DONE');return
        if s['step']%2==0:
            p,length=s['points'][i];pawn.set_actor_location(p+unreal.Vector(-length*.35,0,596),False,True);pc.set_control_rotation(unreal.Rotator(pitch=8,yaw=0,roll=0))
            s['next']=time.perf_counter()+2
        else:unreal.MiningGUIEditorLibrary.capture_preview(pc,str(out/('Passage_%d.png'%i)))
        s['step']+=1
    except Exception:
        unreal.log_error(traceback.format_exc());unreal.unregister_slate_post_tick_callback(handle)
handle=unreal.register_slate_post_tick_callback(tick)
