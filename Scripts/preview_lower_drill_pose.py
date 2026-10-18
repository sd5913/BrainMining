"""Compare framing in disposable MainGame PIE; never save assets."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/PackagePlaytest'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
h=unreal.MiningGUIEditorLibrary;s={'phase':0,'start':time.monotonic(),'next':0};report={}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def finish(error=None):
    s['done']=True;report['error']=error
    (out/'drill-framing.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
poses=[('shared_before',80,-26,0),('lower_30',80,-30,0),('lower_30_pitch',80,-30,-10),('lower_32_pitch',80,-32,-10)]
def pose(i):
    name,x,z,pitch=poses[i]
    for side,y in [('Right',28),('Left',-28)]:
        s['parts']['Drill'+side+'Mount'].set_editor_property('relative_location',unreal.Vector(x,y,z))
        model=s['parts']['Drill'+side+'Model']
        model.set_editor_property('relative_location',unreal.Vector())
        model.set_editor_property('relative_rotation',unreal.Rotator(pitch=pitch))
def tick(dt):
    if s.get('done') or s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<180
        if now<s['next']:return
        if s['phase']==0:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');h.start_floating_preview();s.update(phase=1,next=now+5);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            halls=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)
            if not halls or not halls[0].ready or unreal.GameplayStatics.get_time_seconds(w)<5:return
            for opening in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['next']=now+1;return
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=pc.get_controlled_pawn()
            room=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStartRoom)[0]
            remaining=[p for p in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningStarterPickup) if not p.collected]
            if remaining:
                p=remaining[0].get_actor_location()
                pawn.set_actor_location(unreal.Vector(p.x-125,p.y,room.get_actor_location().z+102),False,True)
                pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
                pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_component_by_class(unreal.CameraComponent).get_world_location(),p))
                h.send_preview_key(pc,'E',True);h.send_preview_key(pc,'E',False)
                s['next']=now+.5;return
            m=pawn.get_component_by_class(unreal.MiningComponent);m.unlock_drill();assert m.activate_drill()
            pc.set_control_rotation(unreal.Rotator());s.update(pc=pc,pawn=pawn,parts={c.get_name():c for c in pawn.get_components_by_class(unreal.SceneComponent)},index=0)
            report['camera']=str(pawn.get_component_by_class(unreal.CameraComponent).get_editor_property('first_person_field_of_view'))
            pose(0);s.update(phase=2,next=now+1);return
        if s['phase']==2:
            assert h.capture_preview(s['pc'],str(out/('main-drill-'+poses[s['index']][0]+'.png')))
            s['index']+=1
            if s['index']==len(poses):finish();return
            pose(s['index']);s['next']=now+1
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
