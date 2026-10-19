"""MainGame renders and real bomb contacts on the two office exit surfaces."""
import unreal,time,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/OptimizationRound3/Gameplay';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
report={'passed':False,'checks':[],'bombs':[]};s={'phase':0,'next':0,'started':time.monotonic()}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
def check(value,label):
    assert value,label
    report['checks'].append(label)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def pose(position,target):
    s['pawn'].set_actor_location(position,False,True)
    s['pc'].set_view_target_with_blend(s['pawn'],0)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],target))
def capture(name):check(helper.capture_preview(s['pc'],str(out/(name+'.png'))),'Captured '+name)
def finish(error=None):
    report.update(passed=not error,error=error)
    (out/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);levels.editor_request_end_play()
    cleanup={'at':time.monotonic()+3}
    def close(dt):
        if ed.get_game_world() or time.monotonic()<cleanup['at']:return
        unreal.unregister_slate_post_tick_callback(cleanup['handle']);levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
    cleanup['handle']=unreal.register_slate_post_tick_callback(close)
def tick(dt):
    if s.get('busy'):return
    s['busy']=True
    try:
        now=time.monotonic()
        if now<s['next']:return
        assert now-s['started']<180,'Gameplay check timeout'
        phase=s['phase']
        if phase==0:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'mining.SaveEnabled 0')
            helper.start_floating_preview();s.update(phase=1,next=now+4);return
        if phase==1:
            w=ed.get_game_world()
            if not w:return
            s['w']=w
            if not actors(unreal.MiningExhibitHall) or not actors(unreal.MiningExhibitHall)[0].ready:return
            for opening in actors(unreal.MiningOpening):
                if not opening.finished:opening.start_game();s['next']=now+1;return
            s.update(pc=unreal.GameplayStatics.get_player_controller(w,0),pawn=unreal.GameplayStatics.get_player_pawn(w,0))
            pickup=next(a for a in actors(unreal.MiningStarterPickup) if a.equipment==unreal.MiningStarterEquipment.PICKAXE)
            target=unreal.SystemLibrary.get_component_bounds(pickup.interaction_box)[0]
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pose(target-unreal.Vector(160,0,64),target);s.update(phase=11,next=now+.3);return
        if phase==11:
            check(actors(unreal.MiningStartRoom)[0].interact(s['pc']),'Normal starter pickaxe interaction releases the tutorial tool lock')
            assert helper.prepare_collection_preview_area(s['pc'],2,False)
            s['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            fields=sorted(actors(unreal.MiningCavernMotes),key=lambda a:a.get_actor_location().x)
            check(len(fields)==2,'Both mushroom chambers have a full dust field')
            for a in fields:check(a.motes.get_instance_count()==600,'Chamber has 600 bounded dust particles')
            pose(unreal.Vector(-11400,-300,6400),unreal.Vector(-10200,0,7100));s.update(phase=2,next=now+2);return
        if phase==2:
            capture('mushroom-dust-1');pose(unreal.Vector(-7250,-250,8750),unreal.Vector(-6100,0,9500));s.update(phase=3,next=now+2);return
        if phase==3:
            capture('mushroom-dust-2');assert helper.prepare_collection_preview_area(s['pc'],1,False)
            stage=actors(unreal.PopupCloseStage)[0];stage.set_actor_tick_enabled(False)
            s['stage']=stage;s['tool']=s['pawn'].get_component_by_class(unreal.MiningBombToolComponent)
            s['tool'].unlock_skill();s['tool'].set_editor_property('bUseInventory',False)
            settings=s['tool'].settings;settings.fuse_seconds=30;s['tool'].settings=settings
            door=next(c for c in stage.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='OfficeBlastDoor')
            target=unreal.SystemLibrary.get_component_bounds(door)[0]
            s.update(target=target,bomb_index=0,phase=4,next=now+.4)
            pose(target-unreal.Vector(440,0,64),target);return
        if phase==4:
            s['pc'].reset_ignore_move_input();s['pc'].reset_ignore_look_input()
            s['tool'].stow();check(s['tool'].toggle_equipped(),'Bomb can be equipped at exit '+str(s['bomb_index']))
            check(s['tool'].throw_bomb(),'Real bomb thrown at exit '+str(s['bomb_index']))
            s.update(bomb=s['tool'].last_thrown_bomb,phase=5,next=now+.1);return
        if phase==5:
            b=s['bomb']
            if not b.stuck:return
            scale=b.model.get_world_scale()
            check((scale-unreal.Vector(1.4,1.4,1.4)).length()<.01,'Bomb retains full 1.4 world scale after contact')
            report['bombs'].append(dict(surface=str(b.collision.get_attach_parent()),scale=list(scale.to_tuple())))
            s.update(phase=6,next=now+.4);return
        if phase==6:
            capture('bomb-exit-'+str(s['bomb_index']));s['bomb'].destroy_actor()
            if s['bomb_index']==0:
                assert helper.prepare_collection_preview_area(s['pc'],2,False)
                gate=next(a for a in actors(unreal.GeneratedMiningRock) if a.get_actor_label()=='MAIN_GAME_OfficeExit_BombOnly')
                target=gate.get_actor_bounds(False)[0]
                pose(target-unreal.Vector(440,0,64),target)
                s.update(target=target,bomb_index=1,phase=4,next=now+.4);return
            finish();return
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
handle=unreal.register_slate_post_tick_callback(tick)
