"""MainGame: real C4 pickup, player bomb breach, reduced crowd and collapse."""
import unreal,json,time,traceback,re
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/OfficeC4';OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':'setup','start':time.monotonic(),'next':0,'busy':False,'pickup':0};report={'passed':False,'checks':[],'crowd_frame_ms':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def key(k,down):helper.send_preview_key(s['pc'],k,down)
def press(k):key(k,True);key(k,False)
def aim(target):s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),target))
def move(p,target=None):
    s['pawn'].set_actor_location(p,False,True);s['move'].stop_movement_immediately();s['move'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    if target:aim(target)
def shot(name):check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Captured '+name)
def finish(error=None):
    if 'pc' in s:
        for k in ['E','G','LeftMouseButton']:key(k,False)
    if 'world' in s:unreal.GameplayStatics.set_global_time_dilation(s['world'],1)
    perf.set_editor_property('bThrottleCPUWhenNotForeground',old);report.update(passed=error is None,elapsed=time.monotonic()-s['start'])
    if error:report.update(error=error,phase=s['phase'])
    (OUT/'runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');s['phase']='done';unreal.unregister_slate_post_tick_callback(callback);levels.editor_request_end_play()
def tick(dt):
    if s['busy'] or s['phase']=='done':return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<180,'Office C4 audit timed out at '+s['phase']
        if s['phase'] in ['crowd','collapse']:report['crowd_frame_ms'].append(dt*1000)
        if now<s['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=s['phase']
        if phase=='setup':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pawn or not pc or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady'):return
            office=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PopupCloseStage)[0];meshes={c.get_name():c for c in office.get_components_by_class(unreal.StaticMeshComponent)}
            s.update(world=world,pawn=pawn,pc=pc,hall=halls[0],office=office,pickup_mesh=meshes['OfficeBombSkillPickup'],crowd=meshes['OfficeFinaleBombs'],move=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent),wallet=pawn.get_component_by_class(unreal.MiningProgressionComponent).get_wallet(),starters=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),start_room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0],phase='aim_starter')
            s['origin']=office.get_actor_location();report['evacuation_seconds']=float(office.evacuation_bomb_seconds);check(office.get_level()==s['hall'].areas[1].source_level.get_loaded_level(),'Office remains a referenced source room')
        elif phase=='aim_starter':
            p=s['starters'][s['pickup']].get_actor_location();move(unreal.Vector(p.x-125,p.y,s['start_room'].get_actor_location().z+102),p);s.update(phase='starter_press',next=now+.6)
        elif phase=='starter_press':
            aim(s['starters'][s['pickup']].get_actor_location());key('E',True);s.update(phase='starter',next=now+.15)
        elif phase=='starter':
            key('E',False);check(s['starters'][s['pickup']].collected,'Real E collects starter equipment '+str(s['pickup']));s['pickup']+=1;s.update(phase='aim_starter' if s['pickup']<2 else 'office',next=now+.3)
        elif phase=='office':
            s['hall'].set_editor_property('linear_campaign',False)
            try:check(s['hall'].enter_area(1),'Enter office through MainGame area management')
            finally:s['hall'].set_editor_property('linear_campaign',True)
            p=s['pickup_mesh'];c=s['crowd'];check(p.static_mesh.get_name()=='SM_StickyBomb' and abs(p.get_editor_property('relative_scale3d').x-1.6)<.001,'Single collectible uses full-detail C4 at 1.6 scale')
            check(c.static_mesh.get_name()=='SM_OfficeFinaleC4' and c.get_editor_property('forced_lod_model')==2,'Crowd selects separate reduced C4 LOD')
            check(c.static_mesh.get_num_triangles(1)<850 and c.get_instance_count()==256,'Reduced C4 geometry and 256-instance budget load from saved asset')
            check(c.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION and not c.get_editor_property('cast_shadow'),'Crowd disables collision and shadows')
            move(s['origin']+unreal.Vector(-400,0,98),s['origin']+unreal.Vector(0,0,124));s['office'].start_office_round();unreal.GameplayStatics.set_global_time_dilation(world,6);s.update(phase='offer',next=now+.4)
        elif phase=='offer':
            if 'Exit phase=2' not in s['office'].get_exit_status():s['next']=now+.4;return
            unreal.GameplayStatics.set_global_time_dilation(world,1);s.update(phase='aim_c4',next=now+2.2)
        elif phase=='aim_c4':
            p=s['pickup_mesh'].get_attach_parent();check(p.is_simulating_physics() and 20<p.get_scaled_sphere_radius()<28,'Larger collectible retains a matching physical sphere')
            target=p.get_world_location();move(unreal.Vector(target.x-110,target.y,s['origin'].z+98),target);s.update(phase='collect',next=now+.4)
        elif phase=='collect':
            shot('01_enlarged_c4_pickup');key('E',True);s.update(phase='collected',next=now+.15)
        elif phase=='collected':
            key('E',False);check('Exit phase=3' in s['office'].get_exit_status(),'Real E acquires the offered C4 without bypassing the office finale');check(not s['pickup_mesh'].is_visible(),'Collecting removes the displayed C4');press('G');s.update(phase='throw',next=now+.2)
        elif phase=='throw':
            press('LeftMouseButton');s.update(phase='place',next=now+.2)
        elif phase=='place':
            bomb=s['bomb'].last_thrown_bomb;check(bool(bomb),'Shared player bomb ability throws its real C4');bomb.set_actor_location(s['origin']+unreal.Vector(968,0,85),False,True);bomb.get_component_by_class(unreal.SphereComponent).set_physics_linear_velocity(unreal.Vector());s.update(phase='breach',next=now+3.6)
        elif phase=='breach':
            check('door_open=1' in s['office'].get_exit_status(),'Actual player bomb explosion breaches office door');press('G');move(s['origin']+unreal.Vector(1135,0,98),s['origin']+unreal.Vector(0,0,120));s['balance']=s['wallet'].balance;s.update(phase='escape',next=now+.3)
        elif phase=='escape':
            check(s['office'].is_office_escaped(),'Physical doorway crossing starts C4 eruption');s.update(phase='crowd',next=now+2)
        elif phase=='crowd':
            c=s['crowd'];status=s['office'].get_exit_status();check('Exit phase=5' in status,'C4 spray is active before room collapse');shot('02_instanced_c4_spray')
            visible=sum(c.get_instance_transform(i,False).scale3d.x>.1 for i in range(c.get_instance_count()));check(12<=visible<=256,'C4 spray uses bounded visible instances')
            check(not c.is_simulating_physics(),'C4 spray has no per-bomb physics simulation');report.update(crowd_status=status,visible_c4=visible,lod_triangles=c.static_mesh.get_num_triangles(1),material_slots=len(c.static_mesh.static_materials),physical_player_bombs=len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningBombActor)));s.update(phase='collapse',next=now+.3)
        elif phase=='collapse':
            if not s['office'].is_office_destroyed():s['next']=now+.3;return
            check(s['crowd'].get_instance_count()==0 and not s['crowd'].is_visible(),'Collapse clears all C4 instances');check(s['wallet'].balance==s['balance'],'Cinematic C4 spray grants no extra coins');check(s['bomb'].is_skill_unlocked(),'Shared bomb ownership survives the finale');report['final_status']=s['office'].get_exit_status();shot('03_c4_finale_ruins');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
