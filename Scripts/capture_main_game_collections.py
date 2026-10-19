"""Final visual review after local Heaven lighting/material repair. No map or save writes."""
import unreal,json,time,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameCollections'
cfg=json.loads((ROOT/'Config/MainGameCollections.json').read_text());auth=json.loads((OUT/'authoring.json').read_text())
promotion=json.loads((OUT/'promotion.json').read_text()) if (OUT/'promotion.json').exists() else {}
test_map=cfg['source_map'] if promotion.get('passed') else cfg['preview_map']
source_hashes={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in auth['source_hashes']}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
s={'phase':0,'after':0,'start':time.monotonic(),'images':[]}
def finish(ok,error=''):
    unreal.unregister_slate_post_tick_callback(handle)
    if ed.get_game_world():levels.editor_request_end_play()
    (OUT/'visual.json').write_text(json.dumps({'passed':ok,'error':error,'images':s['images']},ensure_ascii=False,indent=2),encoding='utf-8')
    cleanup={'after':time.monotonic()+2,'loaded':False}
    def close(dt):
        if time.monotonic()<cleanup['after'] or ed.get_game_world():return
        if not cleanup['loaded']:
            s.clear();levels.load_level('/Engine/Maps/Entry');cleanup.update(loaded=True,after=time.monotonic()+1);return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now-s['start']>150:raise RuntimeError('Visual review timed out')
        if now<s['after']:return
        ph=s['phase']
        if ph==0:
            assert levels.load_level(test_map)
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            unreal.MiningGUIEditorLibrary.start_floating_preview();s.update(phase=1,after=now+4);return
        if ph==1:
            w=ed.get_game_world()
            if not w:return
            hall=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)[0]
            if not hall.ready:return
            for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningOpening):a.start_game()
            s.update(w=w,hall=hall,pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0),phase=2,after=now+5);return
        if ph==2:
            for opening in unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['after']=now+.2;return
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            frame=unreal.Transform(location=unreal.Vector(*auth['heaven']['frame_position']),rotation=unreal.Rotator(yaw=auth['heaven']['yaw']))
            local=lambda x,y,z:unreal.MathLibrary.transform_location(frame,unreal.Vector(x,y,z))
            cassette=unreal.Vector(*next(p['position'] for p in auth['placements'] if p['item_id']==5))
            escape=unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.MiningRoomEscape)[0];ribbon=escape.stage_ribbon.get_actor_location()
            s['views']=[('review_cassette.png',1,cassette+unreal.Vector(35,125,65),cassette)]
            s.update(phase=3,index=0);return
        if ph==3:
            name,index,p,target=s['views'][s['index']]
            assert unreal.MiningGUIEditorLibrary.prepare_collection_preview_area(s['pc'],index,index==3)
            s['pawn'].set_actor_location(p,False,True);s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target))
            s.update(phase=4,after=now+4);return
        if ph==4:
            name=s['views'][s['index']][0]
            assert unreal.MiningGUIEditorLibrary.capture_preview(s['pc'],str(OUT/name))
            if name=='review_cassette.png':
                field=unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.MiningCollectionField)[0]
                tape=next(a for a in field.items if a.item_id==5)
                assert abs(tape.get_actor_rotation().yaw+90)<.01 and abs(tape.get_actor_scale3d().x-.25)<.001
                assert field.target==tape,'Front of upright cassette must be reachable'
                field.collect_target();assert tape.collected and field.inventory[5]==1
            s['images'].append(name);s['index']+=1
            if s['index']<len(s['views']):s['phase']=3;return
            assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in source_hashes.items())
            finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
