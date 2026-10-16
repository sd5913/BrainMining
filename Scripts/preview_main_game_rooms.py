"""Capture the referenced room instances in MainGame without saving PIE edits."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRooms';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text())
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
state={'phase':'setup','i':0,'next':0,'start':time.monotonic(),'busy':False};report={'passed':False,'images':[]}
views=[(1,[-16450,-430,6498],[-15750,0,6540],'08_office_room'),(3,[7600,2500,2400],[14000,3600,3100],'09_echo_cards_room'),(5,[46950,11300,4496],[48200,11750,4550],'10_mirror_museum')]
def finish(error=None):
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old);report.update(passed=error is None,error=error)
    (OUT/'preview.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(callback);state['phase']='done';levels.editor_request_end_play()
def tick(dt):
    if state['busy'] or state['phase']=='done':return
    state['busy']=True
    try:
        now=time.monotonic();assert now-state['start']<75,'Preview timeout'
        if now<state['next']:return
        world=ed.get_game_world()
        if not world:return
        if state['phase']=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall);pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui:return
            state.update(hall=halls[0],pc=pc,pawn=unreal.GameplayStatics.get_player_pawn(world,0),phase='place')
        if state['phase']=='place':
            i,p,target,name=views[state['i']];hall=state['hall'];hall.set_editor_property('linear_campaign',False);assert hall.enter_area(i);hall.set_editor_property('linear_campaign',True)
            pawn=state['pawn'];pawn.set_actor_location(unreal.Vector(*p),False,True);move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent);move.stop_movement_immediately();move.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_component_by_class(unreal.CameraComponent).get_world_location(),unreal.Vector(*target)));state.update(phase='shot',next=now+1)
        elif state['phase']=='shot':
            path=str(OUT/(views[state['i']][3]+'.png'));assert helper.capture_preview(state['pc'],path);report['images'].append(path);state['i']+=1
            if state['i']==len(views):finish()
            else:state.update(phase='place',next=now+.15)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not ed.get_game_world();unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
