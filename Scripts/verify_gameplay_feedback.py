"""Exercise saved maps with the real campaign pawn, jetpack and excavation tools."""
import unreal,json,time,traceback,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/GameplayFeedback'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
auth=json.loads((OUT/'authoring.json').read_text(encoding='utf-8'))
s={'next':0,'busy':False,'start':time.monotonic(),'phase':'load'};report={'passed':False,'checks':[],'transfers':[]}
def v(p):return unreal.Vector(*p)
def xyz(p):return list(p.to_tuple())
def check(ok,name):
    assert ok,name
    report['checks'].append(name);unreal.log('FEEDBACK_CHECK '+name)
def allof(cls):return unreal.GameplayStatics.get_all_actors_of_class(s['w'],cls)
def tagged(tag):return unreal.GameplayStatics.get_all_actors_with_tag(s['w'],tag)
def trace(a,b,ignore=None):
    h=unreal.SystemLibrary.line_trace_single(s['w'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore or [s['pawn']],unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def area(tag):
    index=next(i for i,a in enumerate(s['hall'].areas) if str(a.tag)==tag)
    check(helper.prepare_collection_preview_area(s['pc'],index,False),'Activate '+tag)
def aim(p):s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_actor_eyes_view_point()[0],p))
def pose(p,target=None,fall=False):
    s['pawn'].set_actor_location(p,False,True);s['move'].stop_movement_immediately();s['move'].set_movement_mode(unreal.MovementMode.MOVE_FALLING if fall else unreal.MovementMode.MOVE_FLYING)
    if target is not None:aim(target)
def shot(name):check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Screenshot '+name)
def until(predicate,timeout,label):
    end=time.monotonic()+timeout
    while not predicate():
        assert time.monotonic()<end,label
        yield .05
def run():
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');helper.start_floating_preview()
    while not ed.get_game_world():yield .2
    s['w']=ed.get_game_world();s['hall']=allof(unreal.MiningExhibitHall)[0]
    while not s['hall'].get_editor_property('bReady'):yield .2
    while unreal.GameplayStatics.get_time_seconds(s['w'])<1.5:yield .1
    openings=allof(unreal.MiningOpening)
    for o in openings:o.start_game()
    yield from until(lambda:all(o.get_editor_property('bFinished') for o in openings),20,'Opening did not return control')
    s['pc']=unreal.GameplayStatics.get_player_controller(s['w'],0);s['pawn']=unreal.GameplayStatics.get_player_pawn(s['w'],0);s['move']=s['pawn'].get_component_by_class(unreal.MiningCharacterMovementComponent);s['mine']=s['pawn'].get_component_by_class(unreal.MiningComponent)
    s['hall'].set_actor_tick_enabled(False)
    room=allof(unreal.MiningStartRoom)[0]
    for pickup in allof(unreal.MiningStarterPickup):
        p=pickup.get_actor_location();pose(v([p.x-125,p.y,room.get_actor_location().z+102]),p);yield .5
        helper.send_preview_key(s['pc'],'E',True);yield .15;helper.send_preview_key(s['pc'],'E',False);yield .2
        check(pickup.collected,'Acquire actual starter tool '+pickup.get_name())
    s['phase']='museum'
    area('BM_Campaign_MainGameMuseum');yield 2
    museum=allof(unreal.WenSiMirrorPuzzle)[0];museum.reset_puzzle();yield 1
    lt=tagged('WenSiBalanceLeftTray')[0];rt=tagged('WenSiBalanceRightTray')[0];beam=tagged('WenSiBalanceBeam')[0]
    check(rt.get_actor_location().z<lt.get_actor_location().z-15 and beam.get_actor_rotation().pitch<0,'Initial balance tilts down toward black counterweight')
    origin=museum.get_actor_location();pose(origin+v([1590,-100,130]),origin+v([1590,-430,95]));yield 1;shot('balance_initial')
    mirror=next(a for a in allof(unreal.WenSiMirrorExhibit) if a.get_editor_property('kind')==10);normal=mirror.get_actor_forward_vector();mp=mirror.get_actor_location()
    pose(mp+normal*190+v([0,0,-45]),mp);yield 1.5
    presence=next(a for a in allof(unreal.Actor) if a.get_owner()==mirror);parts=list(presence.get_components_by_class(unreal.StaticMeshComponent))
    check(presence and len(parts)==7,'Mirror creates a soft glowing egg carrying a pickaxe')
    check(all(any(p.get_name()==name for p in parts) for name in ['MirrorEggCore','MirrorEggHalo','MirrorEggGrip','MinerPickaxeHandle','MinerPickaxeHead']),'Egg keeps a readable pickaxe and has no human features')
    check(all(p.get_material(0).get_editor_property('parent').get_name()=='M_MirrorEgg' for p in parts if p.get_name().startswith('MirrorEgg')),'Egg uses feathered glow material')
    check(presence.get_actor_location().distance(s['pawn'].get_actor_location())<1,'Mirror egg follows real player position')
    check(all(p.get_editor_property('visible_in_scene_capture_only') and p.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for p in parts),'Egg only appears in scene capture and has no collision')
    shot('mirror_player');before=presence.get_actor_location();pose(s['pawn'].get_actor_location()+mirror.get_actor_right_vector()*65,mp);yield .5
    check(presence.get_actor_location().distance(before)>60,'Mirror egg tracks lateral movement');shot('mirror_player_side')
    pose(mp+normal*150+v([0,0,-45]),mp);yield .7;shot('mirror_egg_close')
    metal=next(a for a in allof(unreal.WenSiMirrorExhibit) if a.get_editor_property('kind')==8);pose(metal.get_actor_location()+v([170,-170,70]),metal.get_actor_location());yield 1
    arcs=[c for c in metal.get_components_by_class(unreal.StaticMeshComponent) if c.get_material(0) and c.get_material(0).get_name()=='M_ConductiveArc'];check(len(arcs)==18 and any(c.is_visible() for c in arcs),'Conductive word carries visible electric arcs')
    positions=[xyz(c.get_world_location()) for c in arcs];shot('conductive_word');yield 1
    check(any(xyz(c.get_world_location())!=p for c,p in zip(arcs,positions)),'Conductive arcs animate continuously')
    metal.set_editor_property('word',unreal.WenSiMirrorWord.ORDINARY);metal.refresh_art_model();yield .2
    check(all(not c.is_visible() for c in arcs),'Removing conductive word hides electric arcs')
    metal.set_editor_property('word',unreal.WenSiMirrorWord.CONDUCTIVE);metal.refresh_art_model();yield .2
    check(any(c.is_visible() for c in arcs),'Restoring conductive word restores electric arcs')
    result=museum.validate_mirror_puzzle();check(result.startswith('PASS'),'Full museum puzzle: '+result)
    s['phase']='office'
    area('BM_Campaign_MainGameOffice');yield 1
    walls=tagged('BM_Feedback_WallCollision');check(len(walls)==32,'All 32 authored office wall sections use collision')
    for wall in walls:
        c=wall.static_mesh_component;check(c.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION,'Office collision '+wall.get_name())
    for name in ['StaticMeshActor_280','StaticMeshActor_296','StaticMeshActor_297','StaticMeshActor_329']:
        wall=next(a for a in walls if a.get_name()==name);c,e=wall.get_actor_bounds(False);axis=v([1,0,0]) if e.x<e.y else v([0,1,0]);h=trace(c-axis*80,c+axis*80)
        check(h and h[0] and h[9]==wall,'Interior wall physically blocks trace '+name)
    stage=allof(unreal.PopupCloseStage)[0]
    for x in [-16755,-14745]:
        h=trace(v([x-80,0,6500]),v([x+80,0,6500]),[s['pawn'],stage])
        check(not h or not h[0],'Office doorway remains open after collision repair '+str(x))
    pose(v([-15950,-500,6500]),v([-15600,-250,6570]));yield .7;shot('office_collision')
    s['phase']='blast'
    area('BM_Campaign_MainGame02');yield 1
    gate=tagged('BM_Feedback_ThinBlastLayer')[0];center=v([-13760,0,6510]);pose(center-v([450,0,0]),center);yield .7;shot('blast_layer_before')
    h=trace(center-v([200,0,0]),center+v([200,0,0]));check(h and h[9]==gate,'Thin first blast layer closes tunnel')
    bomb=s['pawn'].get_component_by_class(unreal.MiningBombToolComponent);bomb.unlock_skill();check(bomb.can_use(),'Bomb input available after leaving office');check(bomb.toggle_equipped(),'Equip actual bomb');yield .2
    report['bomb_diagnostic']={'count':bomb.bomb_count,'position':xyz(s['pawn'].get_actor_location()),'camera':xyz(s['pc'].player_camera_manager.get_camera_location())}
    check(bomb.throw_bomb(),'Throw actual bomb against first blast layer')
    b=bomb.last_thrown_bomb;b.collision.set_simulate_physics(False);b.set_actor_location(center-v([65,0,0]),False,True);yield 5
    yield from until(lambda:gate.successful_cuts>0,15,'Bomb did not cut first layer')
    bomb.stow();h=trace(center-v([200,0,0]),center+v([200,0,0]));check(not h or not h[0],'Actual bomb opens first blast layer');shot('blast_layer_open')
    s['phase']='platforms'
    platforms=auth['platforms'];check(len(tagged('BM_Feedback_RockPlatform'))==12,'All twelve platforms use irregular rock meshes')
    pose(v([-11200,300,7600]),v([-10300,0,7500]));yield 1;shot('rock_mushrooms')
    for ia,ib in [(3,4),(4,10),(8,9),(9,11)]:
        start=v(platforms[ia]['top']);target=v(platforms[ib]['top']);pose(start+v([0,0,100]),target+v([0,0,90]),True)
        if ib==11:
            report['terminal2_scan']=[]
            for j in range(13):
                p=start+(target-start)*(j/12);p.z=10500
                h=trace(p,p+v([0,0,2000]),[s['pawn']]+list(tagged('BM_Feedback_RockPlatform')))
                report['terminal2_scan'].append({'point':xyz(p),'roof':xyz(h[5]) if h and h[0] else None,'actor':h[9].get_actor_label() if h and h[0] else None})
            report['terminal2_flight']=[]
        yield from until(lambda:s['move'].is_moving_on_ground() and s['move'].jetpack_heat<.02,15,'Cannot stand on mushroom '+str(ia))
        s['move'].set_flight_input_held(ib!=11);crossing=ib==11;airborne=False;deadline=time.monotonic()+18
        while True:
            assert time.monotonic()<deadline,('Jetpack transfer failed',ia,ib,xyz(s['pawn'].get_actor_location()))
            pos=s['pawn'].get_actor_location();delta=target+v([0,0,100])-pos;horizontal=v([delta.x,delta.y,0]);dist=horizontal.length()
            if ib==11:report['terminal2_flight'].append({'pos':xyz(pos),'v':xyz(s['move'].velocity),'heat':s['move'].jetpack_heat,'thrust':s['move'].is_flight_thrust_active()})
            if not s['move'].is_moving_on_ground():airborne=True
            # The final platform sits below diggable ceiling: enter with the
            # capsule's feet just above its lip, without flying into the soil.
            clearance=110 if ib>=10 else 180
            if pos.z>target.z+clearance:crossing=True
            if ib>=10 and not crossing and dist>205:
                s['pawn'].add_movement_input(horizontal.normal(),min(1,(dist-195)/100))
            if crossing:
                if dist>30:s['pawn'].add_movement_input(horizontal.normal(),min(1,dist/160))
                if ib==11:
                    # This landing is lower than the previous cap. Coast beneath
                    # the sloping roof and catch the fall, rather than boost into it.
                    s['move'].set_flight_input_held(dist>65 and pos.z<target.z+140)
                else:s['move'].set_flight_input_held(dist>65 if ib>=10 else pos.z<target.z+230 and dist>100)
                if airborne and s['move'].is_moving_on_ground():
                    h=trace(pos,pos-v([0,0,160]));report['last_landing']={'pair':[ia,ib],'position':xyz(pos),'hit':h[9].get_actor_label() if h and h[0] else None};shot('landing_'+str(ib));check(h and h[0] and h[9].get_actor_label()==platforms[ib]['label'],'Jetpack reaches '+platforms[ib]['label']);break
            yield .01
        report['transfers'].append([ia,ib]);s['move'].set_flight_input_held(False)
        if ib in [10,11]:
            top=target;capname='GeneratedMiningRock_'+('25' if ib==10 else '26');cap=next(a for a in allof(unreal.GeneratedMiningRock) if a.get_name()==capname)
            check(s['move'].is_moving_on_ground(),'Stand below soil on terminal mushroom '+str(ib));aim(top+v([0,0,500]));yield .4;shot('upward_dig_'+str(ib))
            old=cap.successful_cuts;s['mine'].select_pickaxe()
            for attempt in range(20):
                s['mine'].try_mine();yield .65
                if cap.successful_cuts>old:break
            check(cap.successful_cuts>old,'Normal pickaxe digs overhead soil while standing '+str(ib))
    s['phase']='downward_soil'
    area('BM_Campaign_MainGame03');yield 1
    soil=tagged('BM_MuseumDownwardSoil')[0];top=auth['museum_soil']['bottom']+900
    pose(v([47200,11750,top+100]),v([47200,11750,top-400]),True);yield 1
    s['mine'].select_pickaxe();initial=s['pawn'].get_actor_location().z;cuts=soil.successful_cuts;first=False;deadline=time.monotonic()+100;attempt=0
    shot('museum_deep_soil_before')
    while s['pawn'].get_actor_location().z>auth['museum_soil']['bottom']-60:
        assert time.monotonic()<deadline,('Downward digging stalled',s['mine'].status,xyz(s['pawn'].get_actor_location()),soil.successful_cuts)
        # Step over the opening as the chipped rim nudges the capsule sideways.
        # This uses normal movement input, never a teleport into the excavation.
        offset=v([47200,11750,0])-s['pawn'].get_actor_location();offset.z=0
        if offset.length()>35:
            for step in range(8):
                s['pawn'].add_movement_input(offset.normal(),.45);yield .016
        eye=s['pawn'].get_actor_eyes_view_point()[0];aim(eye-v([0,0,1000]))
        # Jagged pickaxe cavities can leave a lip under the capsule while the
        # center is already deep. Look down at that lip and widen the footing.
        h=trace(eye,eye-v([0,0,390]))
        if not h or not h[0] or h[9]!=soil:
            for radius in [55,100,145]:
                candidates=[]
                for j in range(8):
                    p=eye+v([radius*math.cos(j*math.tau/8),radius*math.sin(j*math.tau/8),-340])
                    hit=trace(eye,p)
                    if hit and hit[0] and hit[9]==soil:candidates.append(hit[5])
                if candidates:aim(min(candidates,key=lambda p:p.distance(eye)));break
        s['mine'].try_mine();attempt+=1;yield .65
        if not first and soil.successful_cuts>cuts:
            first=True;h=trace(s['pawn'].get_actor_location()+v([0,0,50]),v([47200,11750,auth['museum_soil']['bottom']-100]))
            check(h and h[0] and h[9]==soil,'First downward cut leaves substantial soil below player');shot('museum_deep_soil_first_cut')
    check(soil.successful_cuts-cuts>=4,'Museum descent requires repeated downward excavation')
    report['downward']={'cuts':soil.successful_cuts-cuts,'swings':attempt,'initial_z':initial,'final_z':s['pawn'].get_actor_location().z}
    yield 2;shot('museum_descent_complete')
    check(s['pawn'].get_actor_location().z>14000,'Player lands in museum after deeper excavation')
    s['phase']='complete'
def finish(error=None):
    report.update(passed=error is None,error=error,phase=s['phase'],seconds=time.monotonic()-s['start']);(OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if s.get('move'):s['move'].set_flight_input_held(False)
    levels.editor_request_end_play();s['done']=True
def tick(dt):
    if s['busy']:return
    if s.get('done'):
        if not ed.get_game_world():
            unreal.unregister_slate_post_tick_callback(handle)
            for key in ['w','hall','pawn','pc','move','mine']:s.pop(key,None)
            levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
        return
    if time.monotonic()<s['next']:return
    s['busy']=True
    try:
        assert time.monotonic()-s['start']<600,'Verification timeout'
        s['next']=time.monotonic()+next(runner)
    except StopIteration:finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
unreal.EditorPythonScripting.set_keep_python_script_alive(True);runner=run();handle=unreal.register_slate_post_tick_callback(tick)
