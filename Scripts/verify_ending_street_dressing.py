"""Render and check the authored street in actual PIE, without saving the level."""
import json
import time
import traceback
import statistics
from pathlib import Path
import unreal

night_review='-EndingNightReview' in unreal.SystemLibrary.get_command_line()
OUT=Path(unreal.Paths.project_dir())/('Saved/Validation/EndingNight' if night_review else 'Saved/Validation/EndingStreet')
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
inspection='-EndingStreetInspection' in unreal.SystemLibrary.get_command_line()
state={'step':0,'next':0,'started':time.monotonic(),'busy':False,'frame_seconds':[]}
report={'passed':False,'checks':[],'images':[]}
def check(value,message):
    assert value,message
    report['checks'].append(message)
def finish(error=None):
    report.update(passed=not error,error=error)
    if state['frame_seconds']:
        samples=sorted(state['frame_seconds'])
        report['street_frame_ms']={'samples':len(samples),'median':1000*statistics.median(samples),
            'p95':1000*samples[int((len(samples)-1)*.95)],'max':1000*max(samples)}
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle)
    levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+8}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle'])
        unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def capture(name):
    check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'Captured '+name)
    report['images'].append(str(OUT/(name+'.png')))
def camera(eye,target):
    cam=state['pc'].get_view_target()
    cam.set_actor_location(unreal.Vector(22000+eye[0],eye[1],eye[2]),False,False)
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(cam.get_actor_location(),
        unreal.Vector(22000+target[0],target[1],target[2])),False)
    cam.camera_component.set_editor_property('field_of_view',75)
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['step']==2 and state.get('director') and state['director'].shot in (0,1):
            state['frame_seconds'].append(dt)
        assert now-state['started']<300,'Street render timed out'
        if now<state['next']:return
        step=state['step']
        if step==0:
            check(levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'),'Loaded authored ending')
            helper.start_floating_preview()
            state.update(step=1,next=now+5)
            return
        world=ed.get_game_world()
        if not world:return
        if step==1:
            ds=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EndingEmergence)
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not ds or not pc or unreal.GameplayStatics.get_time_seconds(world)<4:return
            d=ds[0]
            unreal.SystemLibrary.execute_console_command(world,'DisableAllScreenMessages')
            city=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
                  if a.actor_has_tag('EndingEditableCity')]
            cell=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor)
                  if a.actor_has_tag('EndingEditableCell')]
            check(bool(city) and bool(cell),'Both authored scenery stages loaded')
            check(all(a.get_attach_parent_actor()==d for a in city+cell),'Stage attachments preserved')
            check(all(not a.root_component.is_visible() for a in city),'Street hidden during cell stage')
            check(all(a.root_component.is_visible() for a in cell),'Cell scenery visible at start')
            if night_review:
                night=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
                       if a.actor_has_tag('EndingStreetNightV1') and a!=d]
                expected=152 if d.actor_has_tag('EndingCornerGardensV1') else 124
                check(len(night)==expected,'All '+str(expected)+' night decorations and lights loaded')
                if expected==152:
                    corner=[a for a in night if a.actor_has_tag('EndingCornerGardensV1')]
                    check(len(corner)==28,'Four corner gardens with seven objects each loaded')
                    check(not any('_BenchX' in a.get_actor_label() or '_BenchY' in a.get_actor_label() for a in corner),'Intersection corner benches removed')
                    lamps=[a for a in night if isinstance(a,unreal.PointLight) and 'IntersectionLamp_' in a.get_actor_label()]
                    check(len(lamps)==4 and all(a.point_light_component.get_editor_property('cast_shadows') for a in lamps),'Four intersection lamps cast real dynamic shadows')
                check(all(not a.root_component.is_visible() for a in night),'Night lights and signs hidden during cell stage')
                state['night']=night
            check(sum(a.actor_has_tag('EndingAlignedCrosswalk') for a in city)==4,'Four aligned crossings loaded')
            check(all(a.get_actor_up_vector().z>.999 for a in city
                      if a.actor_has_tag('EndingAlignedCrosswalk')),'All crossing markings lie flat on the road')
            check(all('/EndingEmergence/City/' not in a.static_mesh_component.static_mesh.get_path_name() for a in city),'Cartoon street meshes replaced')
            crowd=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.CrowdInstancer)
                   if a.get_owner()==d]
            check(len(crowd)==1,'One crowd instancer loaded')
            crowd_stage=d.get_component_by_class(unreal.EndingCrowdComponent)
            check(crowd_stage is not None,'Street crowd stage component loaded')
            check((crowd[0].get_actor_location()-crowd_stage.get_world_location()).length()<1,
                  'Crowd instancer is aligned with the street stage')
            state.update(director=d,pc=pc,city=city,cell=cell)
            d.preview_film()
            state.update(step=2,next=now+.5)
            return
        d=state['director']
        if step==2:
            # Let the real rail leave the stranger's opening eye close-up.
            if d.shot==0:return
            check(d.shot==1,'Original city pullback film stage entered')
            check(all(a.root_component.is_visible() for a in state['city']),'Entire authored street revealed by original stage')
            check(all(not a.root_component.is_visible() for a in state['cell']),'Cell hidden during street shot')
            if night_review:check(all(a.root_component.is_visible() for a in state['night']),'Night lights and signs visible during street shot')
            cam=state['pc'].get_view_target()
            report['film_camera']={'location':list(cam.get_actor_location().to_tuple()),
                                   'rotation':str(cam.get_actor_rotation()),'shot':d.shot}
            capture('01-film-street')
            if not inspection:
                state.update(step=6,next=now+1)
                return
            d.set_actor_tick_enabled(False)
            camera((-1600,-2100,1800),(0,0,100))
            state.update(step=3,next=now+4)
            return
        if step==3:
            capture('02-intersection')
            camera((-7000,-8500,13500),(0,0,450))
            state.update(step=4,next=now+3)
            return
        if step==4:
            capture('03-overview')
            camera((-1400,-800,190),(2800,-800,1200))
            state.update(step=5,next=now+3)
            return
        if step==5:
            capture('04-street-details')
            d.set_actor_tick_enabled(True)
            d.set_editor_property('film_speed',4)
            state.update(step=6,next=now+1)
            return
        if step==6:
            if d.shot<3:return
            check(all(not a.root_component.is_visible() for a in state['city']),'Street hides after transition to Earth')
            if night_review:check(all(not a.root_component.is_visible() for a in state['night']),'Night lights and signs hide after street transition')
            finish()
    except Exception:
        finish(traceback.format_exc())
    finally:state['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
