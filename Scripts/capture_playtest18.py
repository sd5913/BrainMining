"""Exercise GM upgrades, real walking decals and collection presentation across map travel."""
import unreal,time,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/Playtest18/Visual';OUT.mkdir(parents=True,exist_ok=True)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);H=unreal.MiningGUIEditorLibrary
s={'start':time.monotonic(),'next':0,'busy':False};r={'passed':False,'checks':[],'images':[]}

def check(name,value):
    assert value,name
    r['checks'].append(name)

def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)

def capture(name):
    check('Capture '+name,H.capture_preview(s['pc'],str(OUT/(name+'.png'))));r['images'].append(name)

def presentation(toilet):
    return dict(body=toilet.body.static_mesh.get_path_name(),lid=toilet.lid.static_mesh.get_path_name(),
                scale=list(toilet.get_actor_scale3d().to_tuple()),body_materials=[x.get_path_name() for x in toilet.body.get_materials()],
                lid_materials=[x.get_path_name() for x in toilet.lid.get_materials()])

def run():
    assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    floor=A.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,49975))
    floor.tags=['Playtest18Floor'];floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.static_mesh_component.set_collision_profile_name('BlockAll');floor.set_actor_scale3d(unreal.Vector(40,18,.5))
    unreal.SystemLibrary.execute_console_command(E.get_editor_world(),'mining.SaveEnabled 0')
    H.start_floating_preview()
    while not E.get_game_world():yield .2
    s['w']=E.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0)
    yield 7
    pc=s['pc'];pawn=pc.get_controlled_pawn();s['pawn']=pawn;H.resize_preview_viewport(pc,1280,720)
    opening=actors(unreal.MiningOpening)[0];opening.start_game()
    while not opening.finished:yield .2
    hall=actors(unreal.MiningExhibitHall)[0];hall.set_actor_tick_enabled(False)
    H.write_preview_state(actors(unreal.MiningStartRoom)[0],'bHeadlampUnlocked','True')
    for light in pawn.get_components_by_class(unreal.LightComponent):
        if light.get_name() in ['MiningLight','MiningFillLight']:
            light.set_visibility(True);light.set_intensity(1.4 if light.get_name()=='MiningLight' else .22)
    gi=unreal.GameplayStatics.get_game_instance(s['w'])
    session=next(x for x in unreal.ObjectIterator(unreal.MiningExperienceSubsystem) if x.get_outer()==gi)
    check('GM opens',session.toggle_developer_menu())
    capture('01-gm-tools')
    for i in range(4):check('GM pickaxe upgrade '+str(i+1),session.developer_grant_tool(0))
    check('GM grants bombs',session.developer_grant_tool(1));check('GM grants drill',session.developer_grant_tool(2))
    session.toggle_developer_menu();yield .5
    model=next(x for x in pawn.get_components_by_class(unreal.StaticMeshComponent) if x.get_name()=='PickaxeModel')
    check('Maximum tier uses the diamond mesh',model.static_mesh.get_name()=='SM_Pickaxe_max')
    check('Diamond head keeps its own cyan material',model.get_material(0).get_name()=='Stylized_Metal_Base4')
    pawn.get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
    pawn.set_actor_location(unreal.Vector(0,0,50100),False,True);pc.set_control_rotation(unreal.Rotator())
    yield 1;capture('02-diamond-pickaxe')
    field=next(x for x in actors(unreal.MiningCollectionField) if x.use_authored_placements)
    check('Seed inventory',H.write_preview_state(field,'Inventory','(1,1,1,1,1,1,1,1,1,1,1,1)'))
    check('Seed cross-map inventory',H.write_preview_state(session,'CampaignCollection','(1,1,1,1,1,1,1,1,1,1,1,1)'))
    field.toggle_catalogue();field.select_item(0);yield .7
    expected=presentation(next(x for x in actors(unreal.HeavenToilet) if x.get_actor_location().z<-9000));r['gameplay_toilet']=expected
    capture('03-gameplay-toilet');field.toggle_catalogue()
    # Walk on an isolated plain floor using real character movement and the
    # component's own left/right material instances, rather than fixture decals.
    move=pawn.get_component_by_class(unreal.CharacterMovementComponent)
    move.set_movement_mode(unreal.MovementMode.MOVE_WALKING);yield 1
    until=time.monotonic()+1.6
    while time.monotonic()<until:
        pawn.add_movement_input(unreal.Vector(1,0,0),1,True);yield 0
    move.stop_movement_immediately();move.disable_movement()
    marks=[c for c in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(c) and c.component_has_tag('MiningFootprint')]
    check('Real walking leaves alternating footprints',len(marks)>=3)
    sides={round(c.get_decal_material().get_scalar_parameter_value('FootSide')) for c in marks}
    check('Footprints alternate mirrored left and right soles',sides=={-1,1})
    r['footprints']=len(marks)
    target=pawn.get_actor_location()-unreal.Vector(110,0,100)
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(pawn.get_actor_eyes_view_point()[0],target))
    yield .3;capture('04-shoe-footprints')
    # The actual successful pickup owns both lines; the subtitle subsystem
    # handles the line gap and duration before showing the next instruction.
    ribbon=H.spawn_preview_collectible(field,11,pawn.get_actor_location()+unreal.Vector(0,0,50))
    target=ribbon.get_actor_location();eye=pawn.get_actor_eyes_view_point()[0]
    pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target));yield .25
    sub=next(x for x in unreal.ObjectIterator(unreal.SubtitleSubsystem) if x.get_outer()==gi);sub.stop_all()
    before=field.inventory[11];field.collect_target()
    check('Pickup acquires the ribbon',field.inventory[11]==before+1);yield .3;capture('05-ribbon-line')
    yield 1.8;capture('06-ribbon-followup')
    field=None;ribbon=None;model=None;marks=[];pawn=None;hall=None;opening=None;move=None;floor=None
    unreal.GameplayStatics.open_level(pc,'/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
    yield 1
    while not E.get_game_world() or 'Lvl_EndingEmergence_Demo' not in E.get_game_world().get_name():yield .2
    s['w']=E.get_game_world();s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0);pc=s['pc']
    while not pc or not pc.get_controlled_pawn():
        yield .2;pc=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pc']=pc
    yield 4
    check('The game instance survives real level travel',session.get_outer()==unreal.GameplayStatics.get_game_instance(s['w']))
    check('Prepare real ending credits',H.prepare_ending_credits_preview(pc,list(session.campaign_collection)))
    yield 1;capture('07-credits')
    field=actors(unreal.MiningCollectionField)[0];field.toggle_catalogue();field.select_item(0);yield .7
    actual=presentation(next(x for x in actors(unreal.HeavenToilet) if x.get_actor_location().z<-9000));r['credits_toilet']=actual
    check('Credits reuse gameplay toilet meshes, materials and scale',actual==expected)
    capture('08-credits-toilet');field.toggle_catalogue()
    r['passed']=True

def finish(error=None):
    if error:r['error']=error
    (OUT/'runtime.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
    unreal.unregister_slate_post_tick_callback(handle);L.editor_request_end_play()
    clean={'at':time.monotonic()+3}
    def close(dt):
        if E.get_game_world() or time.monotonic()<clean['at']:return
        unreal.unregister_slate_post_tick_callback(clean['handle']);s.clear();L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
    clean['handle']=unreal.register_slate_post_tick_callback(close)

def tick(dt):
    if s['busy'] or time.monotonic()<s['next']:return
    s['busy']=True
    try:
        assert time.monotonic()-s['start']<420,'Visual check timed out'
        s['next']=time.monotonic()+next(gen)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
gen=run();handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
