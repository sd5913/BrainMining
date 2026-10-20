"""PIE regressions for pickup cleanup, traversal, breadcrumbs and heaven limits."""
import unreal,time,json,traceback,gc
from pathlib import Path

root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/TraversalFeedback';out.mkdir(parents=True,exist_ok=True)
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
H=unreal.MiningGUIEditorLibrary
state={'phase':'start','start':time.monotonic(),'next':0,'busy':False}
report={'passed':False,'checks':[],'captures':[]}
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground');settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
def check(ok,name):
    assert ok,name
    report['checks'].append(name)
def actors(cls):return unreal.GameplayStatics.get_all_actors_of_class(E.get_game_world(),cls)
def subtitle():
    gi=unreal.GameplayStatics.get_game_instance(E.get_game_world())
    return next(x for x in unreal.ObjectIterator(unreal.SubtitleSubsystem) if x.get_outer()==gi)
def cube(p,size):
    if 'fixture_actors' not in state:state['fixture_actors']=list(actors(unreal.StaticMeshActor))
    a=state['fixture_actors'].pop();c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE)
    a.set_actor_location(p,False,True);a.set_actor_rotation(unreal.Rotator(),False)
    a.set_actor_hidden_in_game(False);a.set_actor_enable_collision(True);c.set_visibility(True)
    c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));c.set_material(0,unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionBedrock'))
    a.set_actor_scale3d(size/100);c.set_collision_profile_name('BlockAll');return a
def shot(name):
    check(H.capture_preview(state['pc'],str(out/(name+'.png'))),'Capture '+name);report['captures'].append(name)
def pose(p,target):
    pawn=state['pawn'];pawn.set_actor_location(p,False,True)
    eye=pawn.get_actor_eyes_view_point()[0];state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
def finish(error=None):
    report.update(passed=not error,error=error,seconds=time.monotonic()-state['start'])
    (out/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    if E.get_game_world():L.editor_request_end_play()
    state.update(phase='cleanup',next=time.monotonic()+2)
def footprints():return [x for x in unreal.ObjectIterator(unreal.DecalComponent) if unreal.SystemLibrary.is_valid(x) and x.component_has_tag('MiningFootprint')]
def tick(dt):
    if state.get('busy'):return
    state['busy']=True
    try:
        now=time.monotonic();phase=state['phase']
        if phase=='walk':state['pawn'].add_movement_input(unreal.Vector(1,0,0),1,True)
        if now<state['next']:return
        if phase=='cleanup':
            if E.get_game_world():return
            gc.collect();settings.set_editor_property('bThrottleCPUWhenNotForeground',throttle)
            unreal.unregister_slate_post_tick_callback(handle);unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            (out/'runtime_done.txt').write_text('done');return
        assert now-state['start']<240,'Timeout at '+phase
        if phase=='start':
            assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');H.start_floating_preview();state.update(phase='ready',next=now+7);return
        w=E.get_game_world()
        if not w:return
        if phase=='ready':
            pc=unreal.GameplayStatics.get_player_controller(w,0)
            if not pc or not pc.get_controlled_pawn():return
            pawn=pc.get_controlled_pawn();state.update(pc=pc,pawn=pawn)
            openings=actors(unreal.MiningOpening)
            if not openings:return
            state['opening']=openings[0];openings[0].start_game();state.update(phase='pickup_setup',next=now+4);return
        if phase=='pickup_setup':
            if not state['opening'].finished:return
            field=next(f for f in actors(unreal.MiningCollectionField) if f.use_authored_placements)
            field.set_editor_property('area_managed',False)
            H.write_preview_state(field,'Inventory','('+','.join(['0']*12)+')')
            for hall in actors(unreal.MiningExhibitHall):hall.set_actor_tick_enabled(False)
            # Put the real pickup in clear air so authored furniture cannot
            # occlude the gameplay trace or gravity move the pawn off the aim.
            point=unreal.Vector(200,0,50120)
            item=H.spawn_preview_collectible(field,1,point)
            state['pawn'].get_component_by_class(unreal.CharacterMovementComponent).disable_movement()
            pose(unreal.Vector(0,0,50070),point);state.update(field=field,item=item,phase='pickup',next=now+.4);return
        if phase=='pickup':
            sub=subtitle();sub.stop_all();state['field'].collect_target()
            check(state['item'].collected,'Ant computer picked up through shared gameplay path')
            segment=H.read_preview_object(sub,'ActiveSegment')
            check(segment is not None and str(segment.lines[0].text)=='蚂蚁一直挖 挖出了复杂的通路','Ant subtitle revised wording')
            state.update(phase='pickup_hint',next=now+4);return
        if phase=='pickup_hint':
            segment=H.read_preview_object(subtitle(),'ActiveSegment')
            check(segment and str(segment.segment_id)=='首次收藏提示','First collection hint follows pickup')
            state.update(phase='pickup_done',next=now+7);return
        if phase=='pickup_done':
            check(H.read_preview_object(subtitle(),'Widget') is None,'Pickup and followup subtitle widgets clean up')
            subtitle().play_segment('收集物拾取_01');subtitle().play_segment('Missing_Queued_Regression')
            state.update(phase='missing_done',next=now+4);return
        if phase=='missing_done':
            check(H.read_preview_object(subtitle(),'Widget') is None,'Missing queued asset cannot strand previous subtitle')
            field=state['field'];H.write_preview_state(field,'Inventory','('+','.join(['1']*12)+')')
            field.toggle_catalogue();state.update(phase='collection',next=now+.8);return
        if phase=='collection':
            field=state['field'];widget=field.catalogue_widget;widget.select_collection_item(1)
            H.resize_preview_viewport(state['pc'],1920,1080);shot('collection_gold_1920')
            state.update(phase='collection_small',next=now+.3);return
        if phase=='collection_small':
            H.resize_preview_viewport(state['pc'],1024,768);state.update(phase='collection_small_shot',next=now+.3);return
        if phase=='collection_small_shot':
            shot('collection_gold_4x3');state['field'].toggle_catalogue()
            H.resize_preview_viewport(state['pc'],1600,900)
            state.update(phase='bomb_setup',next=now+.5);return
        if phase=='bomb_setup':
            H.write_preview_state(actors(unreal.MiningStartRoom)[0],'bPickaxeUnlocked','True')
            bomb=state['pawn'].get_component_by_class(unreal.MiningBombToolComponent);bomb.unlock_skill()
            bomb.set_editor_property('use_inventory',True);H.write_preview_state(bomb,'BombCount','0');H.write_preview_state(bomb,'RechargeProgress','.25');bomb.set_component_tick_enabled(False)
            state.update(phase='bomb_shot',next=now+.4);return
        if phase=='bomb_shot':
            shot('bomb_charge_icon_only');state.update(phase='movement_setup');return
        if phase=='movement_setup':
            for hall in actors(unreal.MiningExhibitHall):hall.set_actor_tick_enabled(False)
            origin=unreal.Vector(0,0,50000);cube(origin-unreal.Vector(0,0,25),unreal.Vector(4000,1800,50))
            cube(origin+unreal.Vector(260,0,14),unreal.Vector(90,300,28))
            pawn=state['pawn'];move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            capsule=pawn.get_component_by_class(unreal.CapsuleComponent)
            check(capsule.get_unscaled_capsule_radius()==30,'Narrower capsule applied after blueprint defaults')
            body_scale=min(state['pawn'].get_actor_scale3d().to_tuple())
            check(move.max_step_height>=60*body_scale-.1,'Step height accounts for player size')
            check(move.get_walkable_floor_angle()>=55,'Excavated slopes allow 55 degree walking')
            foot=pawn.get_component_by_class(unreal.MiningFootprintComponent)
            check(foot and foot.lifetime==15,'Footprints default to fifteen seconds')
            pose(origin+unreal.Vector(0,0,capsule.get_scaled_capsule_half_height()+8),origin+unreal.Vector(500,0,70))
            move.set_movement_mode(unreal.MovementMode.MOVE_WALKING)
            state.update(move=move,phase='walk',next=now+2.2);return
        if phase=='walk':
            state['move'].stop_movement_immediately()
            check(state['pawn'].get_actor_location().x>450,'Character walks over 28 cm protruding soil without a jump')
            marks=footprints();check(len(marks)>=4,'Grounded movement leaves multiple footprints')
            report['footprints_after_walk']=len(marks)
            target=unreal.Vector(250,0,50000)
            eye=state['pawn'].get_actor_eyes_view_point()[0];state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
            state.update(phase='trail_shot',next=now+.3);return
        if phase=='trail_shot':shot('white_footprint_trail');state.update(phase='trail_gone',next=now+16);return
        if phase=='trail_gone':
            check(len(footprints())==0,'Footprints fade and are removed after fifteen seconds')
            H.prepare_collection_preview_area(state['pc'],4,False)
            state.update(phase='heaven',next=now+2);return
        if phase=='heaven':
            shell=next(a for a in actors(unreal.GeneratedMiningRock) if a.get_actor_label()=='COL_Heaven_DiggableDomeShell')
            buffer=next(a for a in actors(unreal.GeneratedMiningRock) if a.get_actor_label()=='COL_Heaven_OuterDiggableBuffer')
            boundary=next(a for a in actors(unreal.DynamicMeshActor) if a.get_actor_label()=='COL_Heaven_OuterUnmineableBoundary')
            o=shell.get_actor_location()
            def trace(a,b):return unreal.SystemLibrary.line_trace_single(w,o+a,o+b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[state['pawn'],buffer],unreal.DrawDebugTrace.NONE,True)
            hit=trace(unreal.Vector(2800,0,1000),unreal.Vector(3400,0,1000))
            check(hit and hit.to_tuple()[0] and hit.to_tuple()[9]==boundary,'Outer side hits non-excavatable boundary')
            # Test a real mineable roof point beyond the original dome, leaving the
            # inner sky and original hemisphere untouched in the editor map.
            check(buffer.mine_sphere(o+unreal.Vector(0,0,4930),120),'Outer roof buffer remains diggable')
            pose(o+unreal.Vector(-231,-3900,180),o+unreal.Vector(-231,-2500,220))
            shot('heaven_entrance_preserved')
            report['boundary_class']=boundary.get_class().get_name()
            hall=actors(unreal.MiningExhibitHall)[0]
            index=next(i for i,a in enumerate(hall.areas) if a.kind==unreal.MiningExperienceKind.MUSEUM)
            H.write_preview_state(state['pawn'].get_component_by_class(unreal.MiningComponent),'bDrillMode','True')
            H.write_preview_state(state['pawn'].get_component_by_class(unreal.MiningBombToolComponent),'bEquipped','True')
            check(H.prepare_collection_preview_area(state['pc'],index,False),'Activate real MainGame museum')
            state.update(phase='museum_setup',next=now+1);return
        if phase=='museum_setup':
            check(not state['pawn'].get_component_by_class(unreal.MiningComponent).drill_mode and not state['pawn'].get_component_by_class(unreal.MiningBombToolComponent).equipped,'Entering museum switches drill/bomb back to pickaxe')
            puzzle=actors(unreal.WenSiMirrorPuzzle)[0]
            state['move'].disable_movement()
            targets=[e for e in actors(unreal.WenSiMirrorExhibit) if e.get_editor_property('kind')<10 and not e.get_editor_property('hidden')]
            state.update(museum_targets=targets,museum_index=0,phase='museum_aim');return
        if phase=='museum_aim':
            index=state['museum_index'];targets=state['museum_targets']
            check(index<len(targets)*4,'A real museum exhibit can be focused')
            target=targets[index//4];center,_=target.get_actor_bounds(False)
            offsets=[unreal.Vector(-170,0,50),unreal.Vector(170,0,50),unreal.Vector(0,-170,50),unreal.Vector(0,170,50)]
            pose(center+offsets[index%4],center)
            state.update(phase='museum_check',next=now+.3);return
        if phase=='museum_check':
            action=state['pc'].interaction_hud.interaction_action
            if str(action)!='思考':state.update(phase='museum_aim',museum_index=state['museum_index']+1);return
            check(True,'Aimed mirror-museum object displays shared Think/E prompt')
            shot('museum_think_prompt')
            H.send_preview_key(state['pc'],'E',True);H.send_preview_key(state['pc'],'E',False)
            state.update(phase='museum_thought',next=now+.3);return
        if phase=='museum_thought':
            check(str(state['pc'].interaction_hud.interaction_action)!='思考','Thinking consumes available target and suppresses duplicate E prompt')
            H.send_preview_key(state['pc'],'Q',True);H.send_preview_key(state['pc'],'Q',False)
            state.update(phase='museum_rethink',next=now+.3);return
        if phase=='museum_rethink':
            check(str(state['pc'].interaction_hud.interaction_action)=='思考','Q stops thinking and restores shared E prompt')
            finish();return
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
handle=unreal.register_slate_post_tick_callback(tick);unreal.EditorPythonScripting.set_keep_python_script_alive(True)
