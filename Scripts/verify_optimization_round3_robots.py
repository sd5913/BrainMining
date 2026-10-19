"""Render the raised opponent from the actual seated player's view."""
import unreal,time,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/OptimizationRound3'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[]}
def check(v,message):
    assert v,message
    report['checks'].append(message)
assert levels.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
review_camera=api.spawn_actor_from_class(unreal.CameraActor,unreal.Vector())
review_camera.tags=['Round2ReviewCamera']
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'next':0,'start':time.monotonic()}
def capture(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Rendered '+name)
def pose(eye,target):
    s['camera'].set_actor_location(eye,False,True)
    s['camera'].set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target),False)
    s['pc'].set_view_target_with_blend(s['camera'])
def finish(ok,error=''):
    report.update(passed=ok,error=error)
    (out/'visual.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    # Let PIE tear down its world before shutting down the editor renderer.
    cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now<s['next']:return
        if now-s['start']>150:raise RuntimeError('Robot visual review timed out')
        if s['phase']==0:
            helper.start_floating_preview();s.update(phase=1,next=now+4);return
        w=ed.get_game_world()
        if not w:return
        if s['phase']==1:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            if not pawn:return
            stage=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage)[0]
            robots=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomRobot)
            floor=next(a for a in robots if a.actor_has_tag('CardRoomFloorRobot'))
            robot=next(a for a in robots if a!=floor)
            camera=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CameraActor) if a.actor_has_tag('Round2ReviewCamera'))
            s.update(pc=pc,pawn=pawn,stage=stage,floor=floor,robot=robot,camera=camera)
            check((floor.get_actor_forward_vector()*-1).dot((unreal.Vector(6700,3600,floor.get_actor_location().z)-floor.get_actor_location()).normal())>.99,'Floor robot faces the player entrance')
            feet=[c for c in floor.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('Foot')]
            for foot in feet:
                center,extent,_=unreal.SystemLibrary.get_component_bounds(foot)
                check(abs(center.z-extent.z)<3,'Floor robot foot rests on ground')
            pose(unreal.Vector(7200,4000,1350),floor.get_actor_location()+unreal.Vector(0,0,850))
            s.update(phase=2,next=now+1);return
        if s['phase']==2:
            capture('floor-robot');pose(unreal.Vector(7100,-2300,2600),unreal.Vector(9900,0,1600));s.update(phase=3,next=now+1);return
        if s['phase']==3:
            capture('left-stair-route')
            s['pc'].set_view_target_with_blend(s['pawn'])
            unreal.SystemLibrary.execute_console_command(w,'BrainMining.ReviewP2',s['pc']);s.update(phase=4);return
        if s['phase']==4:
            r=s['robot']
            if r.get_editor_property('hidden') or str(r.get_editor_property('expression'))!='Idle':return
            feet=[c for c in r.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('Foot')]
            if not all(abs(c.get_editor_property('relative_rotation').pitch-65)<1 for c in feet):return
            check(True,'Battle robot legs hang down in the actual encounter pose')
            check(abs(r.get_actor_location().z-1750)<100,'Battle robot raised 550 cm for torso visibility')
            report['battle_root']=str(r.get_actor_location())
            report['battle_scale']=str(r.get_actor_scale3d())
            capture('battle-seat')
            s['stage'].set_actor_tick_enabled(False)
            p=r.get_actor_location();pose(p+unreal.Vector(-800,-4200,1800),p+unreal.Vector(-100,0,900))
            s.update(phase=5,next=now+1);return
        if s['phase']==5:
            capture('battle-robot-side');finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
