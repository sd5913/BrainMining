"""Render the changed campaign scenes in PIE without saving gameplay or editor state."""
import unreal,time,json,traceback,os
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=Path(os.environ.get('BRAINMINING_CAPTURE_DIR', str(ROOT/'Saved/Validation/PackagePlaytest')));OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
s={'phase':0,'next':0,'start':time.monotonic(),'images':[],'checks':[]}
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def pose(p,target):
    move=s['pawn'].get_component_by_class(unreal.CharacterMovementComponent);move.stop_movement_immediately();move.disable_movement()
    s['pawn'].set_actor_location(p,False,True);s['pc'].set_view_target_with_blend(s['pawn'],0)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target))
def shot(name):
    assert helper.capture_preview(s['pc'],str(OUT/(name+'.png')));s['images'].append(name)
def area(i):assert helper.prepare_collection_preview_area(s['pc'],i,i==3)
def finish(error=None):
    s['done']=True;unreal.unregister_slate_post_tick_callback(handle)
    (OUT/'visual.json').write_text(json.dumps(dict(passed=not error,error=error,images=s['images'],checks=s['checks']),ensure_ascii=False,indent=2),encoding='utf-8')
    levels.editor_request_end_play();cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('busy') or s.get('done'):return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<360,'Capture timeout'
        if now<s['next']:return
        ph=s['phase']
        if ph==0:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            helper.start_floating_preview();s.update(phase=1,next=now+4);return
        if ph==1:
            w=ed.get_game_world()
            if not w:return
            s['w']=w;halls=actors(unreal.MiningExhibitHall)
            if not halls or not halls[0].ready:return
            s.update(pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0))
            for opening in actors(unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['next']=now+1;return
            s['field']=actors(unreal.MiningCollectionField)[0];area(1)
            pose(unreal.Vector(-16810,15,6500),unreal.Vector(-16655,-115,6540));s.update(phase=2,next=now+2);return
        if ph==2:
            shot('office_restored_entry');area(2)
            p=next(p for p in s['field'].authored_placements if p.item_id==9)
            pose(p.approach_transform.translation,p.transform.translation);s.update(phase=3,next=now+2);return
        if ph==3:
            shot('flower_branch_deep_roots');area(4)
            water=actors(unreal.HeavenToiletWater)[0];f=water.spill_coordinate_frame
            pose(unreal.MathLibrary.transform_location(f,unreal.Vector(2580,0,180)),unreal.MathLibrary.transform_location(f,unreal.Vector(2580,900,750)))
            s.update(phase=4,next=now+2);return
        if ph==4:
            shot('heaven_enclosed_sky');area(3);room=actors(unreal.MiningRoomEscape)[0];s['escape']=room
            pose(room.get_editor_property('computer_entry')+unreal.Vector(-1000,0,-100),room.get_editor_property('computer_entry'));s.update(phase=5,next=now+2);return
        if ph==5:
            shot('television_green_glow');area(5);puzzle=actors(unreal.WenSiMirrorPuzzle)[0];s['puzzle']=puzzle
            cover=next(a for a in actors(unreal.WenSiMirrorExhibit) if a.get_editor_property('kind')==7);s['cover']=cover
            p=cover.get_actor_location();pose(p+unreal.Vector(-330,0,0),p);s.update(phase=6,next=now+2);return
        if ph==6:
            shot('museum_gold_window');assert s['puzzle'].skip_to_reward()
            p=s['puzzle'].get_actor_location();pose(p+unreal.Vector(1640,-90,220),p+unreal.Vector(2020,-305,60));s.update(phase=7,next=now+2);return
        if ph==7:
            shot('museum_powered_wires');s['checks'].append('Museum reward unlock and powered circuit complete')
            finish();return
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
