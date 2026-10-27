"""Check live MainGame interactions with an isolated UserDir and saving disabled."""
import json,time,traceback,re,math,unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/NarrativeOct27'
out.mkdir(parents=True,exist_ok=True)
H=unreal.MiningGUIEditorLibrary
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'next':0,'busy':False,'start':time.monotonic()};report={'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
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
    actors(unreal.MiningOpening)[0].start_game();yield 4
    room=actors(unreal.MiningStartRoom)[0];field=actors(unreal.MiningCollectionField)[0]
    point=room.get_actor_transform().transform_location(unreal.Vector(-150,-150,180))
    item=H.spawn_preview_collectible(field,16,point);item.set_actor_tick_enabled(False);item.set_actor_enable_collision(False)
    pose(point+unreal.Vector(-160,0,20),point);yield 2
    shot('skewer-horizontal')
    pose(point+unreal.Vector(-120,-20,65),point);yield 1
    shot('skewer-side')
    item.set_actor_enable_collision(True);pose(point+unreal.Vector(-160,0,20),point);yield .5
    field.collect_target();check(field.inventory[16]==1,'Collect the real skewer before opening its catalogue view')
    field.toggle_catalogue();field.select_item(16);yield 1
    shot('skewer-catalogue')
    parts=field.preview.get_components_by_class(unreal.StaticMeshComponent)
    check(len(parts)>0 and all(abs(p.get_editor_property('RelativeRotation').pitch-70)<.01 for p in parts),'Model steam shapes align perpendicular to the plate')
    def height(part):
        p=part.get_editor_property('RelativeLocation');return -math.sin(math.radians(70))*p.x+math.cos(math.radians(70))*p.z
    before=[height(p) for p in parts];yield .8
    check(any(height(p)-h>4 for p,h in zip(parts,before)),'Model steam travels outward along the plate normal')
    field.toggle_catalogue();yield .2

gen=run()
def finish(error=None):
    report.update(passed=error is None,error=error)
    (out/'skewer-visual.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
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
