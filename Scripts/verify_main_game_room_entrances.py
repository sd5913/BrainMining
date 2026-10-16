"""PIE spatial exposure checks and identical-camera pre/post-entry images."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoomEntrances';cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text())
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
state={'phase':'setup','i':0,'next':0,'start':time.monotonic(),'busy':False};report={'passed':False,'checks':[],'pairs':[]}
def check(ok,msg):
    assert ok,msg;report['checks'].append(msg);(OUT/'exposure_progress.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def enter(index,move=True):
    hall=state['hall'];hall.set_editor_property('linear_campaign',False)
    try:return hall.enter_area(index,move)
    finally:hall.set_editor_property('linear_campaign',True)
def place(pos):
    pawn=state['pawn'];pawn.set_actor_location(pos,False,True);move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent);move.stop_movement_immediately();move.set_movement_mode(unreal.MovementMode.MOVE_FLYING);state['pc'].set_control_rotation(unreal.Rotator(yaw=0))
def finish(error=None):
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old);report.update(passed=error is None,error=error,elapsed=time.monotonic()-state['start'])
    (OUT/'exposure_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROOM_ENTRANCE_EXPOSURE '+json.dumps(report,ensure_ascii=False));unreal.unregister_slate_post_tick_callback(callback);state['phase']='done';levels.editor_request_end_play()
def signature(settings):
    return tuple(str(settings.get_editor_property(k)) for k in ['auto_exposure_method','auto_exposure_bias','auto_exposure_min_brightness','auto_exposure_max_brightness','auto_exposure_apply_physical_camera_exposure','override_auto_exposure_method','override_auto_exposure_bias','override_auto_exposure_min_brightness','override_auto_exposure_max_brightness','override_auto_exposure_apply_physical_camera_exposure'])
def tick(dt):
    if state['busy'] or state['phase']=='done':return
    state['busy']=True
    try:
        now=time.monotonic();assert now-state['start']<130,'Exposure verification timeout at '+state['phase']
        if now<state['next']:return
        world=ed.get_game_world()
        if not world:return
        if state['phase']=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall);pc=unreal.GameplayStatics.get_player_controller(world,0)
            if not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui:return
            hall=halls[0];pawn=unreal.GameplayStatics.get_player_pawn(world,0);state.update(hall=hall,pc=pc,pawn=pawn)
            exposure=list(unreal.GameplayStatics.get_all_actors_with_tag(world,'BM_MainGame_LocalRoomExposure'));check(len(exposure)==4,'Four source exposure profiles localized independently of room activation')
            state['exposures']=exposure
            check(all(a.get_component_by_class(unreal.BoxComponent).get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for a in exposure),'Spatial exposure bounds participate in shape queries even before room activation')
            for p in cfg['mine_origins']:
                point=unreal.Vector(*p)+unreal.Vector(0,0,3000)
                check(all(any(abs(v-c)>e+600 for v,c,e in zip(point.to_tuple(),a.get_actor_location().to_tuple(),a.get_component_by_class(unreal.BoxComponent).get_scaled_box_extent().to_tuple())) for a in exposure),'Local room exposure does not cover the centre of a mining region')
            for i in [1,3,5]:
                tag=str(hall.areas[i].tag);source=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PostProcessVolume) if a.get_level()==hall.areas[i].source_level.get_loaded_level()]
                local=[a.get_component_by_class(unreal.PostProcessComponent) for a in exposure if a.actor_has_tag(tag)]
                check(len(source)==len(local) and all(not a.enabled for a in source),'Room '+str(i)+' has no globally activated source exposure')
                check(sorted(signature(a.settings) for a in source)==sorted(signature(c.settings) for c in local),'Room '+str(i)+' local volumes inherit current original settings')
                check(all(c.enabled and not c.unbound and c.blend_radius==150 for c in local),'Room '+str(i)+' interior profiles stay enabled with short doorway blends')
            office=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PopupCloseOffice)[0]
            group=next(c for c in office.get_components_by_class(unreal.InstancedStaticMeshComponent) if c.get_name()=='OfficeGroup0')
            check(all(abs(abs(group.get_instance_transform(i,False).translation.x)-1100)>1 for i in range(group.get_instance_count())),'Original office box vestibules are removed only in this MainGame instance')
            stage=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PopupCloseStage)[0]
            check(all(not c.is_visible() and c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for c in stage.get_components_by_class(unreal.StaticMeshComponent) if c.get_name().startswith('OfficeSafeExit')),'Original office exit box parts stay hidden and nonblocking')
            room_lights=[c for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if any(a.get_level()==hall.areas[i].source_level.get_loaded_level() for i in [1,3,5]) for c in a.get_components_by_class(unreal.LightComponent)]
            check(bool(room_lights) and all(c.get_editor_property('cast_shadows') for c in room_lights),'Authored room lights cast shadows instead of leaking through clay')
            state.update(phase='place',next=now+.1)
        elif state['phase']=='place':
            j=state['i'];area=[1,3,5][j];s=cfg['generated_connections'][j*2];check(enter(area-1),'Enter mining approach for '+s['id']);state['hall'].set_actor_tick_enabled(False)
            plug=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock) if a.get_actor_label()=='LINK_'+s['id']+'_DiggableSoil')
            check(plug.mine_blast(plug.get_actor_location(),420,.04,27000+j),'Remove entrance clay through normal solid terrain operations')
            pos=unreal.Vector(cfg['room_activation_x'][j]+300,s['a'][1],s['a'][2]+96);place(pos)
            state.update(area=area,spec=s,pos=pos,phase='before',next=now+2)
        elif state['phase']=='before':
            check(state['hall'].active_area==state['area']-1,'Room stays inactive while looking in from the excavated approach')
            local=[a.get_component_by_class(unreal.PostProcessComponent) for a in state['exposures'] if a.actor_has_tag(str(state['hall'].areas[state['area']].tag))]
            camera=state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()
            for c in local:
                bounds=c.get_attach_parent();center=bounds.get_world_location();extent=bounds.get_scaled_box_extent();check(all(abs(p-o)<e for p,o,e in zip(camera.to_tuple(),center.to_tuple(),extent.to_tuple())),'Interior camera receives source exposure independently of gameplay activation')
            path=str(OUT/(state['spec']['id']+'_before_entry.png'));check(helper.capture_preview(state['pc'],path),'Capture room before gameplay activation')
            check(enter(state['area'],False),'Activate the room without moving the camera');place(state['pos']);state.update(before_image=path,phase='after',next=now+2)
        elif state['phase']=='after':
            check(state['hall'].active_area==state['area'],'Room remains active for the same-camera comparison')
            check(state['pawn'].get_actor_location().distance(state['pos'])<1,'Gameplay activation keeps the comparison camera fixed')
            check(all(a.get_component_by_class(unreal.PostProcessComponent).enabled for a in state['exposures']),'No room entry toggles the spatial exposure profiles')
            path=str(OUT/(state['spec']['id']+'_after_entry.png'));check(helper.capture_preview(state['pc'],path),'Capture the identical view after activation')
            report['pairs'].append({'id':state['spec']['id'],'before':state['before_image'],'after':path});state['i']+=1
            if state['i']==3:finish()
            else:state.update(phase='place',next=now+.2)
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
