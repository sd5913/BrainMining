import unreal,time,traceback
from pathlib import Path
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out=Path(unreal.Paths.project_saved_dir())/'Validation/CollectionV03Visual';out.mkdir(parents=True,exist_ok=True)
s={'phase':0,'next':0,'start':time.monotonic()}
def shot(name):unreal.SystemLibrary.execute_console_command(s['w'],'Shot SHOWUI filename="'+str(out/name)+'" -nosuffix')
def pose(p,d=270):
    s['pawn'].set_actor_location(unreal.Vector(p.x-d,p.y,p.z+15),False,True)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],p))
def done():
    unreal.unregister_slate_post_tick_callback(callback)
    if ed.get_game_world():levels.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()
def tick(dt):
    try:
        now=time.monotonic()
        if now-s['start']>240:raise RuntimeError('Visual run timeout')
        if now<s['next']:return
        p=s['phase']
        if p==0:
            assert levels.load_level('/Game/CollectionPrototype/Maps/Lvl_Collection_V03')
            levels.editor_request_begin_play();s['phase']=1;s['next']=now+20
        elif p==1:
            w=ed.get_game_world()
            if not w:return
            f=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningCollectionField)[0]
            s.update(w=w,f=f,pawn=unreal.GameplayStatics.get_player_pawn(w,0),pc=unreal.GameplayStatics.get_player_controller(w,0))
            s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent).disable_movement()
            chemical=next(i for i in f.items[:4] if i.item_id==2)
            s['rock']=min(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-chemical.get_actor_location().y))
            s['surface']=unreal.Vector(460,chemical.get_actor_location().y,150);pose(s['surface'])
            s['phase']=2;s['next']=now+3
        elif p==2:
            shot('01_stain_attached.png');s['phase']=3;s['next']=now+1
        elif p==3:
            s['rock'].mine_sphere(s['surface'],120);s['phase']=4;s['next']=now+2
        elif p==4:
            shot('02_stain_after_cut.png');s['phase']=5;s['next']=now+1
        elif p==5:
            s['rock'].mine_sphere(s['rock'].get_actor_location(),2500);s['phase']=6;s['next']=now+2
        elif p==6:
            shot('03_no_floating_stains.png');s['phase']=7;s['next']=now+1
        elif p==7 and '-StainsOnly' in unreal.SystemLibrary.get_command_line():
            (out/'stains_success.txt').write_text('Final stain appearance checked before and after excavation.');done()
        elif p==7:
            feathers=unreal.GameplayStatics.get_all_actors_with_tag(s['w'],'CollectionFeather')
            s['feather']=feathers[0];s['anchor']=s['feather'].get_actor_location();pose(s['anchor'],240)
            s['birdrock']=min(unreal.GameplayStatics.get_all_actors_of_class(s['w'],unreal.DiggableRock),key=lambda r:abs(r.get_actor_location().y-s['anchor'].y))
            s['phase']=8;s['next']=now+2
        elif p==8:
            shot('04_feather_attached.png');s['phase']=9;s['next']=now+1
        elif p==9:
            s['birdrock'].mine_sphere(s['anchor']+unreal.Vector(3,0,0),65);s['phase']=10;s['next']=now+.8
        elif p==10:
            shot('05_feather_flutter.png');s['phase']=11;s['next']=now+7
        elif p==11:
            pose(s['feather'].get_actor_location(),170);s['phase']=12;s['next']=now+.7
        elif p==12:
            shot('06_feather_landed.png');s['phase']=13;s['next']=now+1
        else:
            (out/'success.txt').write_text('Stain removal and feather attachment, flutter and landing captured.');done()
    except Exception:
        (out/'error.txt').write_text(traceback.format_exc(),encoding='utf-8');done()
callback=unreal.register_slate_post_tick_callback(tick)
