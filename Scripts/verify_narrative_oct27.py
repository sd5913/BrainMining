"""Check live MainGame interactions with an isolated UserDir and saving disabled."""
import json,time,traceback,re,unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/NarrativeOct27'
out.mkdir(parents=True,exist_ok=True)
H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};report={'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def on_mining_attempt(interval):
    s['mining_attempts']+=1

def check(ok,label):
    assert ok,label
    report['checks'].append(label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def key(name):H.send_preview_key(s['pc'],name,True);H.send_preview_key(s['pc'],name,False)
def pose(eye,target):
    offset=s['pawn'].get_actor_eyes_view_point()[0]-s['pawn'].get_actor_location()
    s['pawn'].set_actor_location(eye-offset,False,True);s['move'].stop_movement_immediately()
    s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def shot(name):check(H.capture_preview(s['pc'],str(out/(name+'.png'))),'Capture '+name)
def run():
    check(levels.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load production map')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
    H.start_floating_preview();yield 20
    s['w']=ed.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0)
    s['pawn']=unreal.GameplayStatics.get_player_pawn(s['w'],0);s['move']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent)
    gi=unreal.GameplayStatics.get_game_instance(s['w'])
    s['sub']=next(o for o in unreal.ObjectIterator(unreal.SubtitleSubsystem) if o.get_outer()==gi)
    room=actors(unreal.MiningStartRoom)[0];opening=actors(unreal.MiningOpening)[0]
    opening.start_game();yield 4
    check(opening.get_editor_property('bFinished'),'Opening completes')
    for pickup in actors(unreal.MiningStarterPickup):
        point=pickup.get_actor_location();pose(point+unreal.Vector(-125,0,50),point);yield .3
        check(room.interact(s['pc']),'Collect starter tool for input/audio regression')
    s['mining_attempts']=0
    s['mining']=s['pawn'].get_component_by_class(unreal.MiningComponent)
    s['mining'].on_mining_attempt.add_callable(on_mining_attempt)
    before=s['mining_attempts'];key('LeftMouseButton');yield .5
    check(s['mining_attempts']>before,'Pickaxe input works before entering a computer')
    for name,text,offset in [('Computer','时间到了，该出门干活了……',(-130,0,30)),('Plant','祝我有好收获。',(70,-60,40)),('Book','遇到好点子就收下来吧！',(-90,-70,65))]:
        actor=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'StartRoomView_'+name)[0]
        center=actor.get_actor_bounds(False)[0]
        if name=='Computer':center=unreal.SystemLibrary.get_component_bounds(actor.get_component_by_class(unreal.SkeletalMeshComponent))[0]
        pose(center+unreal.Vector(*offset),center);yield .3
        check(room.interact(s['pc']),'Real interaction '+name)
        check(str(s['sub'].get_active_text())==text,'Displayed text '+name)
        yield .2;shot('start-'+name.lower())
    yield 6
    check(not str(s['sub'].get_active_text()) and s['sub'].get_pending_segment_count()==0,'Notebook ends without the C-key follow-up')
    s['sub'].stop_all();key('C');yield .5
    field=next(a for a in actors(unreal.MiningCollectionField) if a.get_editor_property('bCatalogueOpen'))
    check(sum(field.inventory)==0,'Catalogue is genuinely empty')
    check(not str(s['sub'].get_active_text()),'Opening the empty catalogue adds no hint')
    shot('empty-collection')
    H.send_preview_focused_key(s['pc'],'C',True);H.send_preview_focused_key(s['pc'],'C',False);yield .3
    saves=next(o for o in unreal.ObjectIterator(unreal.MiningSaveSubsystem) if o.get_outer()==gi)
    check(saves.prototypes_found==0,'First prototype is not awarded in the first passage')
    chemistry=unreal.load_asset('/Game/Subtitles/Collectibles/DA_CollectiblePickup_02')
    check(str(chemistry.lines[0].text)=='化学元素通过反应产生新的物质 进而形成大千世界','Saved chemistry pickup copy')
    field=actors(unreal.MiningCollectionField)[0]
    point=room.get_actor_transform().transform_location(unreal.Vector(-150,-150,180))
    skewer=H.spawn_preview_collectible(field,16,point)
    check(skewer is not None,'Build horizontal skewer model')
    skewer.set_actor_tick_enabled(False)
    skewer.set_actor_enable_collision(False)
    pose(point+unreal.Vector(-95,-110,100),point);yield 1
    shot('skewer-horizontal')
    skewer.destroy_actor()
    check(H.prepare_collection_preview_area(s['pc'],1,False),'Enter office')
    stage=actors(unreal.PopupCloseStage)[0]
    yield 1
    check('phase=0' in stage.get_session_status() and saves.prototypes_found==1,'Office entrance restores the original prototype trigger')
    s['sub'].stop_all()
    device=next(a for a in actors(unreal.PopupOfficeDevice) if a.get_editor_property('DeviceIndex')==0)
    transform=device.get_actor_transform()
    surface=H.read_preview_object(device,'Surface')
    pose(transform.transform_location(unreal.Vector(-190,0,170)),surface.get_world_location());yield .4
    check(saves.prototypes_found==1,'Workstation approach keeps the prototype awarded at the office entrance')
    check(str(s['sub'].get_active_text())=='工作是一个能引发共鸣的话题','Office narration still starts at the workstation')
    s['entry_samples']=[];s['entry_recording']=True;s['office_stage']=stage
    key('E');yield 1
    s['entry_recording']=False
    report['computer_entry']=s['entry_samples']
    moving=[f for f in s['entry_samples'] if 'view=1 ' in f['status']]
    check(len(moving)>=6,'First computer entry blends over multiple rendered frames')
    check(max(f['dt'] for f in moving)<.2,'First computer entry has no frame stall above 200 ms')
    check(not any(f['warnings'] for f in s['entry_samples'] if 'view=1 ' in f['status'] or 'view=2 ' in f['status']),'No scene warning overlays flash during computer entry or operation')
    desktop=H.read_preview_object(device,'Desktop')
    check(desktop.get_front_popup_body()=='21天gamejam游戏创作主题公布！','Production office begins with the announcement')
    shot('office-announcement')
    before=s['mining_attempts'];key('LeftMouseButton');yield .5
    check(s['mining_attempts']==before,'Computer mouse input cannot broadcast a pickaxe swing or its audio')
    yield 20
    check(desktop.get_front_popup_body()=='21天gamejam游戏创作主题公布！','No click: normal slow opening keeps the announcement')
    shot('office-manual-theme-required')
    while float(re.search(r'elapsed=([0-9.]+)',stage.get_session_status())[1])<46:yield .2
    check(desktop.get_front_popup_body()=='【涌现】','Long-idle fallback reveals the theme after the slow opening')
    shot('office-long-idle-theme')
    while float(re.search(r'elapsed=([0-9.]+)',stage.get_session_status())[1])<56:yield .2
    check(desktop.get_front_popup_phase()>=0,'Later messages eventually appear after the fallback theme')
    shot('office-late-fallback')
    while float(re.search(r'elapsed=([0-9.]+)',stage.get_session_status())[1])<64.8:yield .01
    report['burst_frames']=[]
    for frame in range(24):
        status=stage.get_session_status()
        visible=[a for a in actors(unreal.PopupCloseFlyingWindow) if not a.get_editor_property('bHidden')]
        if 'phase=2' in status and 'view=0' not in status:
            check(not visible,'No slabs cross the computer exit blend '+str(frame))
        report['burst_frames'].append({'frame':frame,'status':status.split('\n')[0],'airborne':len(visible)})
        shot('burst-%02d'%frame)
        yield .04
    yield 1
    check('view=0' in stage.get_session_status(),'Forced computer exit completes')
    check(any(not a.get_editor_property('bHidden') for a in actors(unreal.PopupCloseFlyingWindow)),'Prepared popup models emit after the camera exits')
    check(int(re.search(r'Source0 emitted=(\d+)',stage.get_session_status())[1])>0,'First computer emits textured windows, not only printer paper')
    check(int(re.search(r'Source1 emitted=(\d+)',stage.get_session_status())[1])>0,'Second computer also emits its prepared windows')
    shot('burst-settled-view')
    before=s['mining_attempts'];key('LeftMouseButton');yield .5
    check(s['mining_attempts']>before,'Leaving the computer restores normal pickaxe input')
    report['passed']=True
gen=run()
def finish(error=None):
    report.update(passed=error is None,error=error)
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if s.get('mining'):s['mining'].on_mining_attempt.remove_callable(on_mining_attempt)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old)
    unreal.unregister_slate_post_tick_callback(handle)
    if ed.get_game_world():levels.editor_request_end_play()
    shutdown_after=time.monotonic()+3
    def shutdown(dt):
        if ed.get_game_world() or time.monotonic()<shutdown_after:return
        unreal.unregister_slate_post_tick_callback(shutdown_handle)
        # Retire the streamed production world before shutting down the editor.
        levels.load_level('/Engine/Maps/Entry')
        unreal.SystemLibrary.quit_editor()
    shutdown_handle=unreal.register_slate_post_tick_callback(shutdown)
def tick(dt):
    if s.get('entry_recording'):
        p=s['pc'].player_camera_manager.get_camera_location()
        s['entry_samples'].append({'dt':dt,'position':[p.x,p.y,p.z],'status':s['office_stage'].get_session_status().split('\n')[0],'warnings':sum(H.read_preview_object(d,'Warning').is_visible() for d in actors(unreal.PopupOfficeDevice))})
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        assert time.monotonic()-s['start']<180,'Narrative verification timeout'
        s['next']=time.monotonic()+next(gen)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
