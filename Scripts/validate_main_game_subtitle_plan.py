"""Validate saved data and real overlaps/events in the MainGame PIE copy only."""
import json, time, traceback
from pathlib import Path
import unreal
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out = Path(unreal.Paths.project_dir(), 'Saved/Validation/SubtitleTriggers/plan_runtime.json')
report = dict(passed=False, checks=[])
state = dict(phase=0, started=time.monotonic(), busy=False)
def check(ok, text):
    assert ok, text
    report['checks'].append(text)
def move(segment):
    state['subtitles'].stop_all()
    state['pawn'].set_actor_location(state['boxes'][segment].get_actor_location(), False, True)
    state['pawn'].get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_FLYING)
def position(segment):
    check(state['boxes'][segment].get_component_by_class(unreal.BoxComponent).is_overlapping_actor(state['pawn']), 'Player overlaps ' + segment)
    check(any(s.get_outer() == state['subtitles'] for s in unreal.ObjectIterator(unreal.SoundWaveProcedural)), 'Procedural speech created for ' + segment)
def tick(dt):
    if state['busy']: return
    state['busy'] = True
    try:
        assert time.monotonic()-state['started'] < 180, 'PIE validation timeout'
        w = editor.get_game_world()
        if not w or unreal.GameplayStatics.get_time_seconds(w) < 4 or time.monotonic() < state.get('next',0): return
        phase = state['phase']
        if not state.get('opening_started'):
            for opening in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening): opening.start_game()
            state.update(opening_started=True,next=time.monotonic()+4)
            return
        if phase == 0:
            ds = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.MainGameSubtitleDirector)
            check(len(ds)==1, 'One subtitle director loaded')
            d = ds[0]
            report['director_tick_enabled']=d.is_actor_tick_enabled()
            report['director_tags']=[str(t) for t in d.tags]
            report['paused']=unreal.GameplayStatics.is_game_paused(w)
            boxes = {str(a.get_editor_property('segment_id')):a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SubtitleTrigger)}
            check(len(boxes)==5 and '0001' not in boxes, 'Five position triggers and no 0001')
            check(all('Lvl_MainGame_SubtitleTriggers' in a.get_path_name() for a in boxes.values()), 'All position triggers belong to subtitle sublevel')
            segments = d.get_editor_property('segments')
            check(len(segments)==17 and len({str(s.segment_id) for s in segments})==17, '17 unique segments referenced for cooking')
            check(all(l.audio is None for s in segments for l in s.lines), 'All 27 lines have no audio asset')
            check(not unreal.EditorAssetLibrary.does_asset_exist('/Game/Subtitles/DA_0001'), 'Legacy subtitle asset removed')
            gi = unreal.GameplayStatics.get_game_instance(w)
            sub = next(s for s in unreal.ObjectIterator(unreal.SubtitleSubsystem) if s.get_outer()==gi)
            state.update(d=d, boxes=boxes, pawn=unreal.GameplayStatics.get_player_pawn(w,0), subtitles=sub)
            move('离开初始房')
        elif phase == 1:
            position('离开初始房'); move('卡牌玩法1')
        elif phase == 2:
            position('卡牌玩法1')
            echo=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.EchoPassageStage)[0]
            echo.emit_echo(echo.get_actor_location(),True)
        elif phase == 3:
            check(not state['d'].has_triggered('盲人敲地玩法'), 'Echo landing keeps gameplay narration silent')
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            check(unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(pc,3,False),'Activate journey in PIE fixture')
            escape=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningRoomEscape)[0]
            cards=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.CardRoomStage)[0]
            escape.set_editor_property('warning_seconds',.1)
            escape.call_method('OnRewardClaimed',(cards.room_id,))
        elif phase == 4:
            if not state['d'].has_triggered('水涌玩法'):
                state['next']=time.monotonic()+.5; return
            check(state['d'].has_triggered('卡牌玩法2') and state['d'].has_triggered('水涌玩法'), 'Flood dispatches card ending and water narration')
            check(unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(unreal.GameplayStatics.get_player_controller(w,0),5,False),'Activate museum in PIE fixture')
            state['d'].set_editor_property('museum_hint_seconds',1.5)
            move('文思玩法1')
        elif phase == 5:
            position('文思玩法1')
            state['next']=time.monotonic()+2
        elif phase == 6:
            check(state['d'].has_triggered('文思玩法2'), 'Museum timer dispatches hint before endpoint')
            move('文思玩法3')
        elif phase == 7:
            position('文思玩法3')
            progression=state['pawn'].get_component_by_class(unreal.MiningProgressionComponent)
            wallet=progression.get_wallet()
            for mineral in ['2','4','6','9']: check(wallet.add_mineral(mineral,1000),'Fund test mineral '+mineral)
            for level in range(1,5):
                check(progression.purchase(0,level), 'Successful real pickaxe purchase '+str(level))
                check(state['d'].has_triggered('镐子升级'+str(level)), 'Pickaxe narration '+str(level))
            check(not progression.purchase(0,4), 'Duplicate purchase rejected')
            unreal.GameplayStatics.get_all_actors_of_class(w,unreal.PopupCloseStage)[0].start_office_round()
            check(state['d'].has_triggered('电脑弹窗玩法1'), 'Office start dispatches narration')
            report['passed']=True
            finish(); return
        state['phase']+=1
        if phase != 5: state['next']=time.monotonic()+.5
    except Exception:
        report['error']=traceback.format_exc(); finish()
    finally: state['busy']=False
def finish():
    out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(state['handle'])
    levels.editor_request_end_play()
    unreal.log('SUBTITLE_PLAN_VALIDATION '+json.dumps(report,ensure_ascii=False))
assert not editor.get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
state['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
