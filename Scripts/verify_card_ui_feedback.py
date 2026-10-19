"""Exercise the real card inputs and capture uncluttered UI in MainGame."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'Validation/CardUIFeedback';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[]};s={'phase':0,'start':time.monotonic(),'next':0,'busy':False}
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def check(ok,msg):
    assert ok,msg
    report['checks'].append(msg)
def cap(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Captured '+name)
def key(name):
    helper.send_preview_key(s['pc'],name,True);helper.send_preview_key(s['pc'],name,False)
def space():
    helper.send_preview_focused_key(s['pc'],'SpaceBar',True);helper.send_preview_focused_key(s['pc'],'SpaceBar',False)
def step(p,delay=1):s.update(phase=p,next=unreal.GameplayStatics.get_time_seconds(s['w'])+delay)
def finish(ok,error=''):
    report.update(passed=ok,error=error);(out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s['busy']:return
    s['busy']=True
    try:
        assert time.monotonic()-s['start']<360,'Review timed out'
        if s['phase']==0:
            segment=unreal.load_asset('/Game/Subtitles/Collectibles/DA_CollectiblePickup_11');line=unreal.SubtitleLine();line.set_editor_property('text','好漂亮的彩带');segment.set_editor_property('lines',[line]);assert unreal.EditorAssetLibrary.save_loaded_asset(segment,False)
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            helper.start_floating_preview();s['phase']=1;return
        w=ed.get_game_world()
        if not w:return
        s['w']=w
        if unreal.GameplayStatics.get_time_seconds(w)<s['next']:return
        p=s['phase']
        if p==1:
            if not actors(unreal.MiningExhibitHall) or not actors(unreal.MiningExhibitHall)[0].ready:return
            for opening in actors(unreal.MiningOpening):
                if not opening.finished:opening.start_game();step(1,1);return
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0);s.update(pc=pc,pawn=pawn)
            pickup=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
            target=unreal.SystemLibrary.get_component_bounds(pickup.interaction_box)[0]
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pawn.set_actor_location(target-unreal.Vector(160,0,64),False,True);pc.set_view_target_with_blend(pawn,0);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target))
            step(100,.3);return
        if p==100:
            pc=s['pc'];pawn=s['pawn']
            check(actors(unreal.MiningStartRoom)[0].interact(pc),'Starter pickaxe unlocked through interaction')
            bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent);bomb.unlock_skill();check(bomb.toggle_equipped(),'Bomb equipped normally before entering Echo')
            assert helper.prepare_collection_preview_area(pc,2,False)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            echo=actors(unreal.EchoPassageStage)[0];pawn.set_actor_location(echo.get_actor_transform().transform_location(unreal.Vector(0,0,120)),False,True);echo.emit_echo(pawn.get_actor_location(),True)
            s['bomb']=bomb;step(2,.5);return
        if p==2:
            check(not s['bomb'].equipped,'Entering landed Echo room stows a previously held bomb')
            stage=actors(unreal.CardRoomStage)[0];s['stage']=stage
            s['pawn'].set_actor_location(stage.get_actor_transform().transform_location(unreal.Vector(-75,0,215)),False,True)
            s['cards']=s['pc'].get_component_by_class(unreal.CardRoomInteractionComponent)
            step(3,1);return
        if p==3:
            s['cards'].interact();step(4,.35);return
        if p==4:
            check(not s['pc'].mining_gui.is_visible(),'Mining inventory and abilities are hidden while seated')
            cap('01-matching');step(5,2.1);return
        if p==5:cap('02-match-success');step(6,1.7);return
        if p==6:cap('03-laser-hand');key('Gamepad_FaceButton_Left');step(7,.3);return
        if p==7:
            check(str(s['pc'].interaction_hud.interaction_action)=='出牌','Selected card uses shared play prompt')
            cap('04-selected-laser');space();step(8,5.0);return
        if p==8:cap('05-poison-hand');key('Gamepad_FaceButton_Left');step(9,.3);return
        if p==9:space();step(10,1.3);return
        if p==10:cap('06-poison-subtitle');step(11,3.6);return
        if p==11:
            cap('07-fire-laser-hand');key('Gamepad_FaceButton_Left');key('Gamepad_DPad_Right');key('Gamepad_FaceButton_Left');step(12,.3);return
        if p==12:space();step(13,1.3);return
        if p==13:cap('08-fire-subtitle');step(14,3.6);return
        if p==14:
            check(str(s['pc'].interaction_hud.interaction_action)=='全选','Final hand first offers select all')
            cap('09-overlapping-final-hand');space();step(15,.4);return
        if p==15:
            check(str(s['pc'].interaction_hud.interaction_action)=='出牌','First Space selects all without playing')
            cap('10-final-hand-selected');space();step(16,1.4);return
        if p==16:cap('11-robot-farewell');step(17,3);return
        if p==17:cap('12-victory-no-panel');s['cards'].interact();step(18,4);return
        if p==18:
            escape=actors(unreal.MiningRoomEscape)[0];loc=s['pawn'].get_actor_location();loc.z=escape.water_start_z-500
            s['pawn'].set_actor_location(loc,False,True);step(19,.35);return
        if p==19:cap('13-drowning');finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
