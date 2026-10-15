"""PIE visual and excavation checks for all four localized clay transitions."""
import unreal,json,time,traceback,bisect,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameDirt'
sys.path.insert(0,str(ROOT/'Scripts'))
from main_game_route_sources import route_root
ROUTES=route_root(ROOT)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
lib=unreal.MaterialEditingLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
report={'passed':False,'checks':[],'stages':[]}
state={'phase':'setup','start':time.monotonic(),'next':0,'busy':False,'index':0,'pickup_index':0}
STAGES=[('TunnelV1','BM_MainGame_V1'),('TunnelCavernV2','BM_MainGame_CavernV2'),('TunnelStalagmiteV3','BM_MainGame_StalagmiteV3'),('TunnelNeuralV4','BM_MainGame_NeuralV4')]
def check(ok,msg):
    assert ok,msg
    report['checks'].append(msg)
def key(k,on):helper.send_preview_key(state['pc'],k,on)
def press(k):key(k,True);key(k,False)
def move(p):
    state['pawn'].set_actor_location(p,False,True);state['movement'].stop_movement_immediately()
def look(p):
    camera=state['pawn'].get_component_by_class(unreal.CameraComponent)
    state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(),p))
def shot(name):
    assert helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),name
def point(path,s):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)));t=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])
    a,b=path['points'][i-1:i+1];p=[x+(y-x)*t for x,y in zip(a,b)]
    return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def finish(error=None):
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    if state.get('pc'):
        for k in ['Tab','V','G','SpaceBar','W','LeftMouseButton']:key(k,False)
    report.update(passed=error is None,elapsed_seconds=time.monotonic()-state['start'])
    if error:report.update(error=error,phase=state['phase'])
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_DIRT_RUNTIME '+json.dumps(report,ensure_ascii=False))
    state['phase']='done';unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done' or now<state['next']:return
        assert now-state['start']<140,'Dirt verification timed out'
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
            rooms=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)
            if not pawn or not rooms or not pc.mining_gui:return
            state.update(world=world,pc=pc,pawn=pawn,room=rooms[0],movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),
                pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)))
            bodies=pawn.get_components_by_class(unreal.SkeletalMeshComponent)
            check(len(bodies)==2 and all(c.get_editor_property('hidden_in_game') for c in bodies),'Both MainGame skeletal bodies remain hidden after loading the saved Blueprint')
            check(all(not c.get_editor_property('cast_shadow') and not c.get_editor_property('cast_hidden_shadow') for c in bodies),'Hidden bodies cast no body shadow')
            check(pawn.get_actor_enable_collision() and pawn.get_component_by_class(unreal.CameraComponent).is_active(),'Player collision and camera remain active')
            state.update(phase='aim_pickup',next=now+.2)
        elif phase=='aim_pickup':
            pickup=state['pickups'][state['pickup_index']];p=pickup.get_actor_location()
            move(unreal.Vector(p.x-125,p.y,state['room'].get_actor_location().z+102));look(p)
            state.update(phase='collect_pickup',next=now+.25)
        elif phase=='collect_pickup':
            pickup=state['pickups'][state['pickup_index']]
            press('E');check(pickup.collected,'Starter equipment pickup '+str(state['pickup_index'])+' still works')
            state['pickup_index']+=1
            state.update(phase='aim_pickup' if state['pickup_index']<2 else 'tools',next=now+.25)
        elif phase=='tools':
            effects=state['pawn'].get_component_by_class(unreal.MiningToolEffectsComponent)
            check(effects.pickaxe_mesh.is_visible() and not effects.pickaxe_mesh.get_editor_property('hidden_in_game'),'First-person pickaxe remains visible with the body hidden')
            state['effects']=effects
            state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            state.update(phase='region',next=now+.2)
        elif phase=='region':
            stage,tag=STAGES[state['index']]
            region=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) if a.actor_has_tag(tag))
            path=json.loads((ROUTES/stage/'centerlines.json').read_text(encoding='utf-8'))[0]
            plan=json.loads((ROUTES/stage/'placement_plan.json').read_text(encoding='utf-8'))['objects'][0]
            origin=region.get_actor_location()+region.cutters[0].local_transform.translation
            arc=plan['start_arc_m'];p=origin+point(path,arc-4.5)
            move(p);state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            state.update(region=region,guide=region.generated_guide_layers[0],path=path,origin=origin,arc=arc,stage=stage,
                target=origin+point(path,arc+1.0),phase='frame',next=now+.6)
        elif phase=='frame':
            look(state['target']);state.update(phase='before_cut',next=now+.7)
        elif phase=='before_cut':
            region=state['region'];mat=region.rock_material
            check('/DirtTransition/MI_DirtWall_' in mat.get_path_name(),state['stage']+' uses its own dirt-transition wall material')
            check(lib.get_material_instance_scalar_parameter_value(mat,'MudCount')==32,state['stage']+' covers every one of its sixteen blockages')
            check(all(g.rock_material.get_name()=='MI_ClayBlockage' for g in region.generated_guide_layers),state['stage']+' preserves all sixteen excavatable clay cores')
            camera=state['pawn'].get_component_by_class(unreal.CameraComponent);eye=camera.get_world_location()
            h=unreal.SystemLibrary.line_trace_single(world,eye,state['target'],unreal.TraceTypeQuery.ECC_VISIBILITY,True,[state['pawn']],unreal.DrawDebugTrace.NONE,True)
            check(h and h.to_tuple()[0] and h.to_tuple()[9]==state['guide'],state['stage']+' original blockage collision remains in place')
            shot('%02d_%s_before'%(state['index']+1,state['stage']))
            hit=h.to_tuple()[5];direction=state['target']-eye;direction=direction/max(direction.length(),.001)
            state['initial_triangles']=state['guide'].triangle_count
            check(state['guide'].mine_blast(hit+direction*50,140,.22,8700+state['index']),state['stage']+' blockage still accepts excavation')
            state.update(phase='after_cut',next=now+.5)
        elif phase=='after_cut':
            g=state['guide']
            if g.successful_cuts==0:return
            check(g.triangle_count!=state['initial_triangles'],state['stage']+' excavation changes terrain geometry')
            shot('%02d_%s_excavated'%(state['index']+1,state['stage']))
            tangent=point(state['path'],state['arc']+1)-point(state['path'],state['arc']-1)
            right=unreal.Vector(-tangent.y,tangent.x,0);right=right/max(right.length(),.001)
            look(state['origin']+point(state['path'],state['arc']-2)+right*420)
            state.update(phase='side_view',next=now+.35)
        elif phase=='side_view':
            g=state['guide']
            shot('%02d_%s_wall_transition'%(state['index']+1,state['stage']))
            g.reset_rock()
            check(g.successful_cuts==0 and g.triangle_count==state['initial_triangles'],state['stage']+' reset restores the original geometry')
            check(g.rock_material.get_name()=='MI_ClayBlockage',state['stage']+' reset preserves clay material')
            report['stages'].append(dict(stage=state['stage'],wall=state['region'].rock_material.get_path_name(),triangles=state['initial_triangles']))
            state['index']+=1
            state.update(phase='region' if state['index']<4 else 'junction',next=now+.2)
        elif phase=='junction':
            plug=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.get_actor_label()=='STARTROOM_Junction_DiggableBlockage')
            check(plug.rock_material.get_name()=='MI_ClayBlockage','New StartRoom connection uses the same clay appearance')
            move(unreal.Vector(-6780,0,6100));look(unreal.Vector(-6350,0,6150))
            state.update(phase='junction_shot',next=now+.6)
        elif phase=='junction_shot':
            shot('05_start_room_junction')
            press('V');state.update(phase='drill',next=now+.25)
        elif phase=='drill':
            check(state['effects'].drill_mesh.is_visible(),'First-person drill remains visible with the body hidden')
            press('V');press('G');state.update(phase='bomb',next=now+.25)
        elif phase=='bomb':
            check(state['pawn'].get_component_by_class(unreal.MiningBombToolComponent).equipped,'Bomb tool still equips with the body hidden')
            press('G')
            check(len(next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningTreasureField)).items)==125,'All 125 mineral deposits remain loaded')
            finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
