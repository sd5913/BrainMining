"""Render the shared card room in its smaller P2 map; MainGame flow has a separate native test."""
import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'Validation/CardUIFeedback';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[]};s={'phase':0,'start':time.monotonic(),'next':0,'busy':False}
(out/'runtime.json').write_text(json.dumps(report),encoding='utf8')
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def check(ok,msg):
    assert ok,msg
    report['checks'].append(msg)
def cap(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Captured '+name)
def key(name):
    helper.send_preview_focused_key(s['pc'],name,True);helper.send_preview_focused_key(s['pc'],name,False)
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
            segment=unreal.load_asset('/Game/Subtitles/Collectibles/DA_CollectiblePickup_11')
            check(str(segment.get_editor_property('lines')[0].get_editor_property('text'))=='好漂亮的彩带','Ribbon pickup uses the requested subtitle')
            assert levels.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            helper.start_floating_preview();s['phase']=1;return
        w=ed.get_game_world()
        if not w:return
        s['w']=w
        if unreal.GameplayStatics.get_time_seconds(w)<s['next']:return
        p=s['phase']
        if p==1:
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            if not pawn:return
            s.update(pc=pc,pawn=pawn)
            stage=actors(unreal.CardRoomStage)[0];s['stage']=stage
            local=unreal.Vector(-75,0,215) if stage.get_editor_property('tabletop_entry') else unreal.Vector(-295,0,100)
            pawn.set_actor_location(stage.get_actor_transform().transform_location(local),False,True)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pc.set_view_target_with_blend(pawn,0);pc.set_mining_gui_enabled(True)
            s['cards']=pc.get_component_by_class(unreal.CardRoomInteractionComponent)
            step(3,1);return
        if p==3:
            s['cards'].interact();step(4,.35);return
        if p==4:
            check(not s['pc'].mining_gui.is_visible(),'Mining inventory and abilities are hidden while seated')
            cap('01-matching');step(5,2.1);return
        if p==5:cap('02-match-success');step(6,1.7);return
        if p==6:
            unreal.SubtitleLibrary.play_subtitle_segment(s['pc'],'卡牌玩法1')
            cap('03-laser-hand');key('Gamepad_FaceButton_Left');s['cards'].toggle_card_selection(0);step(7,.3);return
        if p==7:
            check(str(s['pc'].interaction_hud.interaction_action)=='出牌','Selected card uses shared play prompt')
            cap('04-selected-laser');space();step(8,5.0);return
        if p==8:cap('05-poison-hand');s['cards'].toggle_card_selection(0);step(9,.3);return
        if p==9:space();step(10,1.3);return
        if p==10:cap('06-poison-subtitle');step(11,3.6);return
        if p==11:
            cap('07-fire-laser-hand');s['cards'].toggle_card_selection(0);s['cards'].toggle_card_selection(1);step(12,.3);return
        if p==12:space();step(13,1.3);return
        if p==13:cap('08-fire-subtitle');step(14,3.6);return
        if p==14:
            check(str(s['pc'].interaction_hud.interaction_action)=='全选','Final hand first offers select all')
            cap('09-overlapping-final-hand');space();step(15,.4);return
        if p==15:
            check(str(s['pc'].interaction_hud.interaction_action)=='出牌','First Space selects all without playing')
            cap('10-final-hand-selected');space();step(16,1.4);return
        if p==16:cap('11-robot-farewell');step(17,3);return
        if p==17:cap('12-victory-no-panel');s['cards'].interact();step(18,6);return
        if p==18:
            cap('13-water-target')
            escape=actors(unreal.MiningRoomEscape)[0];loc=s['pawn'].get_actor_location();loc.z=escape.get_editor_property('water_start_z')-500
            s['pawn'].set_actor_location(loc,False,True);step(19,.35);return
        if p==19:
            check(actors(unreal.MiningRoomEscape)[0].phase==unreal.MiningEscapePhase.FAILED,'Drowning reaches the simplified failure screen')
            cap('14-drowning');finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
handle=unreal.register_slate_post_tick_callback(tick)
