"""Exercise the actual MainGame input, cameras, interactions and saved entrance geometry."""
import unreal,json,time,traceback,math
from pathlib import Path
OUT=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback16';H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};report={'passed':False,'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,label):
    assert ok,label
    report['checks'].append(label);unreal.log('FEEDBACK16 '+label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def tag(t):return unreal.GameplayStatics.get_all_actors_with_tag(s['w'],t)
def key(k,down=None):
    if down is None:H.send_preview_key(s['pc'],k,True);H.send_preview_key(s['pc'],k,False)
    else:H.send_preview_key(s['pc'],k,down)
def pose(eye,target):
    pawn=s['pawn'];pc=s['pc'];offset=pawn.get_actor_eyes_view_point()[0]-pawn.get_actor_location();pawn.set_actor_location(eye-offset,False,True)
    s['move'].stop_movement_immediately();s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def shot(name):check(H.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Capture '+name)
def area(i):check(H.prepare_collection_preview_area(s['pc'],i,False),'Activate area '+str(i))
def decals():return s['opening'].get_components_by_class(unreal.DecalComponent)
def run():
    check(levels.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load latest MainGame')
    H.start_floating_preview()
    while not ed.get_game_world():yield .2
    s['w']=ed.get_game_world()
    while not actors(unreal.MiningOpening):yield .2
    s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pawn']=s['pc'].get_controlled_pawn();s['move']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent)
    opening=actors(unreal.MiningOpening)[0];s['opening']=opening
    while H.read_preview_object(opening,'Menu') is None:yield .1
    yield 1
    H.resize_preview_viewport(s['pc'],1280,720);yield .4
    shot('01_opening_view')
    opening.start_game();samples=[]
    while not opening.get_editor_property('bFinished'):
        t=unreal.GameplayStatics.get_time_seconds(s['w']);p=s['pc'].player_camera_manager.get_camera_location();samples.append([t,p.x,p.y,p.z]);yield 0
    yield .15
    p=s['pc'].player_camera_manager.get_camera_location();samples.append([unreal.GameplayStatics.get_time_seconds(s['w']),p.x,p.y,p.z])
    speeds=[math.dist(a[1:],b[1:])/(b[0]-a[0]) for a,b in zip(samples,samples[1:]) if b[0]>a[0]+.00001]
    report['opening_max_speed_cm_s']=max(speeds);report['camera_samples']=samples
    check(max(speeds)<400,'Opening camera stays continuous through the pawn handoff')
    check(s['pc'].get_view_target()==s['pawn'],'Opening returns to actual player camera')
    room=actors(unreal.MiningStartRoom)[0];s['hall']=actors(unreal.MiningExhibitHall)[0];s['hall'].set_actor_tick_enabled(False)
    door=actors(unreal.MainGameDoor)[0];before=unreal.SystemLibrary.get_component_bounds(door.door_mesh)[0];outward=(before-room.get_actor_location());outward.z=0;outward=outward.normal()
    door.open_door();yield .95
    after=unreal.SystemLibrary.get_component_bounds(door.door_mesh)[0]
    report['door_outward_displacement']=unreal.MathLibrary.dot_vector_vector(after-before,outward)
    check(report['door_outward_displacement']>10,'Bedroom door swings away from room interior')
    field=actors(unreal.MiningCollectionField)[0];key('I');yield .15;check(not field.catalogue_open,'I does not open collection')
    key('C');yield .15;check(field.catalogue_open,'C still opens collection');key('C');yield .2
    pick=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
    target=pick.get_actor_location();pose(target+unreal.Vector(-100,0,70),target);yield .3
    check(room.interact(s['pc']),'Pick up actual starter pickaxe')
    s['pc'].set_control_rotation(unreal.Rotator(yaw=180));yield .35
    check(len(decals())==0,'Pickaxe pickup while looking away does not show title')
    target=opening.get_editor_property('door_title_location');pose(target-unreal.Vector(180,0,0),target);yield .5
    check(len(decals())==1,'Aiming at exit dirt reveals exactly one title decal');shot('02_title_on_dirt')
    comp=tag('StartRoomView_Computer')[0];center=unreal.SystemLibrary.get_component_bounds(comp.get_component_by_class(unreal.SkeletalMeshComponent))[0]
    for name in ['Computer','Robot','Book']:
        options=tag('StartRoomView_'+name)
        a=min(options,key=lambda a:a.get_actor_bounds(False)[0].distance(center));target=a.get_actor_bounds(False)[0]
        pose(target+unreal.Vector(-110,-100,65),target);yield .4
        meshes=a.get_components_by_class(unreal.MeshComponent)
        check(any(m.get_editor_property('render_custom_depth') and m.get_editor_property('custom_depth_stencil_value')==1 for m in meshes),'Common white outline for '+name)
        shot('03_outline_'+name.lower())
        s['pc'].set_control_rotation(unreal.Rotator(pitch=70,yaw=180));yield .25
        check(not any(m.get_editor_property('render_custom_depth') for m in meshes),'Outline clears when looking away from '+name)
    area(3);yield .5
    echo=actors(unreal.EchoPassageStage)[0]
    entrance=next(c for c in echo.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='EntranceRock')
    t=entrance.get_instance_transform(0,False)
    check(abs(t.translation.x+450)<.1 and abs(t.scale3d.x-5)<.01,'Echo floor abuts terrain without the old 60cm overlap')
    origin=echo.get_actor_location();pose(origin+unreal.Vector(-590,0,1030),origin+unreal.Vector(245,0,1000));yield .5;shot('04_echo_join')
    echo.emit_echo(origin,True);yield .4
    director=actors(unreal.MainGameSubtitleDirector)[0]
    check(not director.has_triggered('盲人敲地玩法'),'Echo landing does not dispatch gameplay narration')
    gi=unreal.GameplayStatics.get_game_instance(s['w']);sub=next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==gi)
    sub.stop_all();sub.play_segment('收集物拾取_06');yield .25
    segment=H.read_preview_object(sub,'ActiveSegment')
    check(segment and str(segment.get_editor_property('segment_id'))=='收集物拾取_06','Collectible narration remains available inside echo passage')
    check(abs(unreal.get_default_object(unreal.load_class(None,'/Script/BrainMining.SubtitleSettings')).get_editor_property('VoiceVolume')-.45)<.01,'Subtitle audio gain configured at 45 percent (silent test device)')
    area(5);yield .6
    puzzle=actors(unreal.WenSiMirrorPuzzle)[0];puzzle.reset_puzzle();yield .3
    s['hall'].set_actor_tick_enabled(True);s['move'].set_movement_mode(unreal.MovementMode.MOVE_NONE)
    for k in ['S','K','I','P']:key(k,True)
    yield .85
    check(not field.catalogue_open,'SKIP chord never opens catalogue')
    key('I',False);yield .2;key('I',True);yield 2.4
    check(s['pawn'].get_actor_scale3d().x>.5,'Releasing a SKIP key resets the three second hold')
    yield 1
    check(s['pawn'].get_actor_scale3d().x<.5,'Holding all four SKIP keys for three seconds shrinks the player')
    local=s['pawn'].get_actor_location()-puzzle.get_actor_location();report['skip_local_position']=list(local.to_tuple())
    check(2210<local.x<2440 and abs(local.y+350)<125,'SKIP ends inside the mouse gallery')
    for k in ['S','K','I','P']:key(k,False)
    shot('05_skip_mouse_gallery')
    s['hall'].set_actor_tick_enabled(False)
    area(1);yield .5
    office=actors(unreal.PopupCloseStage)[0];office.start_office_round()
    origin=office.get_actor_location();pose(origin+unreal.Vector(-300,-300,170),origin+unreal.Vector(0,0,150))
    unreal.GameplayStatics.set_global_time_dilation(s['w'],5)
    deadline=time.monotonic()+65
    while 'bomb_visible=1' not in office.get_exit_status():
        assert time.monotonic()<deadline,'Office bomb did not spawn'
        yield .2
    unreal.GameplayStatics.set_global_time_dilation(s['w'],1);yield .5
    pickup=next(c for a in actors(unreal.Actor) for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeBombSkillPickup')
    target=pickup.get_world_location();pose(target+unreal.Vector(-180,-160,130),target);yield .5
    shot('06_office_bomb_waypoint')
    check(pickup.is_visible(),'Bomb guide accompanies the visible, uncollected skill')
    report['passed']=True

gen=run()
def tick(dt):
    if s.get('busy') or time.monotonic()<s.get('next',0):return
    s['busy']=True
    try:
        assert time.monotonic()-s['start']<220,'Validation timeout'
        delay=next(gen);s['next']=time.monotonic()+delay
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
def finish(error=None):
    if error:report.update(passed=False,error=error);unreal.log_error(error)
    (OUT/'main_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
    unreal.unregister_slate_post_tick_callback(handle)
    if ed.get_game_world():levels.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
