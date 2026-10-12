"""Rendered cold-start test with real pickaxe calls at the reported entrance and veins."""
import json, time, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
echo_only='-MiningEchoOnly' in unreal.SystemLibrary.get_command_line()
keep_editor='-MiningKeepEditor' in unreal.SystemLibrary.get_command_line()
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
state=dict(start=time.perf_counter(),phase='wait',hits=[],frames=[],scenario=0)
def quit_tick(dt):
    if editor.get_game_world(): return
    state['shutdown']+=1
    if state['shutdown']>=5:
        unreal.unregister_slate_post_tick_callback(state['quit_handle'])
        state.clear()
        if not keep_editor:
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
            unreal.SystemLibrary.quit_editor()
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    data={k:v for k,v in state.items() if k not in ('pawn','pc','mining','field','scenarios','start','last','next','pending_operation')}
    report_name='MiningEchoVerification.json' if echo_only else 'MiningStabilityVerification.json'
    (root/'Saved'/report_name).write_text(json.dumps(data,indent=2,default=str))
    unreal.log('MINING_STABILITY_RESULT '+json.dumps({k:v for k,v in data.items() if k!='frames'},default=str))
    for key in ('pawn','pc','mining','field','scenarios','pending_operation'):state.pop(key,None)
    levels.editor_request_end_play()
    state['shutdown']=0
    state['quit_handle']=unreal.register_slate_post_tick_callback(quit_tick)
def tick(dt):
    now=time.perf_counter()
    try:
        if now-state['start']>240: raise RuntimeError('Rendered mining test timed out')
        world=editor.get_game_world()
        if not world:return
        if state['phase']=='wait':
            pawn=unreal.GameplayStatics.get_player_character(world,0)
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not halls or not halls[0].ready:return
            field=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningTreasureField)[0]
            # Verify the serialized layout loaded by the game, not just the
            # in-memory editor result before saving the map.
            density_ratios=[]
            for placement in field.get_editor_property('authored_placements'):
                counts=[0,0,0]
                for support in placement.fragment_supports:
                    distance=(support-placement.transform.translation).length()
                    for shell,(low,high) in enumerate(((0,80),(80,160),(160,225))):
                        if low<=distance<high:counts[shell]+=1
                densities=[n/(high**3-low**3) for n,(low,high) in zip(counts,((0,80),(80,160),(160,225)))]
                assert densities[0]>densities[1]>densities[2],'Saved interior vein is not denser toward the core'
                density_ratios.append([densities[0]/densities[1],densities[1]/densities[2]])
            state['saved_density_check']=dict(deposits=len(density_ratios),minimum_inner_to_middle=min(r[0] for r in density_ratios),minimum_middle_to_outer=min(r[1] for r in density_ratios))
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            scenarios=[]
            # Reported problem wall, camera approaching its cave-facing side.
            for x in (-180,0,180):
                surface=unreal.Vector(-15714.58998+x,100,4944.96428)
                for advance in (0,95):
                    scenarios.append(('entrance',surface+unreal.Vector(0,-210+advance,0),surface+unreal.Vector(0,250,0)))
            # Separated deposits avoid aiming through holes made in a neighbour.
            placements=[p for p in field.get_editor_property('authored_placements') if str(p.area_tag)=='BM_Campaign_Mine01'][::4][:6]
            for i,p in enumerate(placements):
                s=p.surface.translation
                n=unreal.MathLibrary.transform_location(p.surface,unreal.Vector(1,0,0))-s
                for advance in (0,90,180):scenarios.append(('vein_'+str(i),s+n*(210-advance),p.transform.translation))
            state.update(pawn=pawn,pc=pc,field=field,mining=pawn.get_component_by_class(unreal.MiningComponent),scenarios=[] if echo_only else scenarios,phase='position',next=now+3,last=now)
            return
        frame=(now-state['last'])*1000
        state['frames'].append(frame);state['last']=now
        if 'pending' in state:state['hits'][state.pop('pending')]['following_frame_ms']=frame
        if 'pending_operation' in state:
            operation=state['pending_operation'];rock=operation['rock']
            assert now-operation['started']<3,'Accepted excavation never completed'
            if rock.get_editor_property('cut_pending'):return
            after=[i.get_editor_property('crystal_clues').get_instance_count() for i in state['field'].items]
            hit=state['hits'][operation['index']]
            hit.update(changed=rock.successful_cuts>operation['cuts'],removed=sum(operation['before'])-sum(after),status=rock.last_status,commit_latency_ms=(now-operation['started'])*1000)
            assert all(a<=b for a,b in zip(after,operation['before'])),'Background cut spawned new fragments'
            assert hit['changed'],'Accepted hit was silently dropped: '+str(hit)
            state.pop('pending_operation')
        if now<state['next']:return
        if state['phase'].startswith('echo_'):
            effects=state['pawn'].get_component_by_class(unreal.MiningToolEffectsComponent)
            line_mode=effects.get_editor_property('echo_pickaxe_lines')
            expected=state['phase']!='echo_door'
            assert line_mode==expected,'Shared pickaxe did not switch at the passage boundary'
            unreal.MiningGUIEditorLibrary.capture_preview(state['pc'],str((root/('Saved/'+state['phase']+'.png')).resolve()))
            mesh=effects.pickaxe_mesh
            state.setdefault('echo_styles',[]).append(dict(phase=state['phase'],lines=line_mode,mesh=mesh.get_path_name(),materials=[m.get_path_name() if m else None for m in mesh.get_materials()],overlay=str(mesh.get_overlay_material()),components=[dict(name=c.get_name(),visible=c.is_visible(),mesh=str(c.static_mesh),materials=[str(m) for m in c.get_materials()]) for c in state['pawn'].get_components_by_class(unreal.StaticMeshComponent)]))
            if state['phase']=='echo_return':finish();return
            door=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningGiantRoomEntrance)[0]
            if state['phase']=='echo_passage':
                position=unreal.MathLibrary.transform_location(door.get_actor_transform(),unreal.Vector(-300,0,-90))
                state['phase']='echo_door'
            else:
                echo=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EchoPassageStage)[0]
                position=unreal.MathLibrary.transform_location(echo.get_actor_transform(),unreal.Vector(800,0,100))
                state['phase']='echo_return'
            state['pawn'].set_actor_location(position,False,True)
            state['pc'].set_control_rotation(door.get_actor_rotation())
            state['next']=now+8
            return
        if state['phase']=='position':
            if state['scenario']>=len(state['scenarios']):
                successful=[h for h in state['hits'] if h['changed']]
                state['successful_cuts']=len(successful)
                if not echo_only:
                    assert len(successful)>=12,'Too few real cuts; empty swings do not count'
                    assert sum(h['changed'] for h in state['hits'] if h['scenario']=='entrance')>=2,'Problem entrance not excavated'
                    assert max(h['wall_ms'] for h in state['hits'])<250,'Game thread still stalls'
                    assert max(h['following_frame_ms'] for h in state['hits'])<500,'Post-cut rendering still stalls'
                    assert sum(h['removed'] for h in state['hits'])>0,'No crystal removal exercised'
                    unreal.SystemLibrary.execute_console_command(world,'memreport -full',state['pc'])
                hall=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)[0]
                # Bypass progression only in this disposable PIE validation world.
                was_linear=hall.get_editor_property('linear_campaign')
                hall.set_editor_property('linear_campaign',False)
                assert hall.enter_area(3)
                hall.set_editor_property('linear_campaign',was_linear)
                state['pawn'].get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
                echo=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.EchoPassageStage)[0]
                position=unreal.MathLibrary.transform_location(echo.get_actor_transform(),unreal.Vector(800,0,100))
                state['pawn'].set_actor_location(position,False,True)
                state['pc'].set_control_rotation(echo.get_actor_rotation())
                echo.emit_echo(position,True)
                state['phase']='echo_passage';state['next']=time.perf_counter()+8
                return
            if state['scenario'] in (7,8,9):
                unreal.MiningGUIEditorLibrary.capture_preview(state['pc'],str((root/f'Saved/InteriorCut_{state["scenario"]-6}.png').resolve()))
            name,eye,target=state['scenarios'][state['scenario']]
            state['pawn'].set_actor_location(eye-unreal.Vector(0,0,64),False,True)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,target))
            state['phase']='cut';state['next']=now+.9
            return
        if state['scenario'] in (0,6):
            unreal.MiningGUIEditorLibrary.capture_preview(state['pc'],str((root/f'Saved/MiningStability_{state["scenario"]}.png').resolve()))
        items=state['field'].items
        before=[i.get_editor_property('crystal_clues').get_instance_count() for i in items]
        target=state['mining'].get_editor_property('target_rock')
        cuts=target.successful_cuts if target else 0
        started=time.perf_counter();ok=state['mining'].try_mine();elapsed=(time.perf_counter()-started)*1000
        after=[i.get_editor_property('crystal_clues').get_instance_count() for i in items]
        state['hits'].append(dict(scenario=state['scenarios'][state['scenario']][0],ok=ok,changed=bool(target and target.successful_cuts>cuts),rock=target.get_name() if target else None,wall_ms=elapsed,removed=sum(before)-sum(after),status=state['mining'].status))
        state['pending']=len(state['hits'])-1
        if isinstance(target,unreal.GeneratedMiningRock) and target.get_editor_property('cut_pending'):
            state['pending_operation']=dict(rock=target,cuts=cuts,before=before,index=len(state['hits'])-1,started=started)
            state['hits'][-1]['deferred']=True
        assert all(a<=b for a,b in zip(after,before)),'Swing spawned new fragments'
        if state['scenario']==6:
            rocks=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GeneratedMiningRock)
            chunks=[c for r in rocks for c in r.get_editor_property('chunks')]
            state['terrain_chunks']=dict(total=len(chunks),visible=sum(c.mesh.is_visible() for c in chunks))
        state['scenario']+=1;state['phase']='position';state['next']=time.perf_counter()+.65
    except Exception:
        state['error']=traceback.format_exc();finish()
handle=unreal.register_slate_post_tick_callback(tick)
unreal.MiningGUIEditorLibrary.start_floating_preview()
