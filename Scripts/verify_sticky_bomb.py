"""Real G/LMB throws: stick to floor/wall/ceiling/rigid body, then detonate at 3s."""
import json,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
out=Path(unreal.Paths.project_saved_dir())/'Validation/StickyBomb';out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'checks':[],'cases':[]};state={'phase':'setup','started':time.monotonic(),'busy':False,'index':0}
base=unreal.Vector(0,-20000,6000)
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
cases=[('floor',-45),('wall',0),('ceiling',65),('moving-body',0)]
def valid(obj):return obj is not None and unreal.SystemLibrary.is_valid(obj)
def vec(v):return [v.x,v.y,v.z]
def check(value,label):
    assert value,label
    report['checks'].append(label)
def press(key):helper.send_preview_key(state['pc'],key,True);helper.send_preview_key(state['pc'],key,False)
def cube(name,location,scale):
    cls=unreal.StaticMeshActor;w=state['w'];before=set(unreal.GameplayStatics.get_all_actors_of_class(w,cls))
    unreal.SystemLibrary.execute_console_command(w,'Summon /Script/Engine.StaticMeshActor',state['pc'])
    actor=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(w,cls) if a not in before)
    actor.set_editor_property('tags',['BM_PlayerEffect',name]);mesh=actor.static_mesh_component
    mesh.set_mobility(unreal.ComponentMobility.MOVABLE);mesh.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    actor.set_actor_location(location,False,True);actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),True);actor.set_actor_scale3d(scale);mesh.set_collision_profile_name('BlockAll')
    return actor
def finish(passed,reason=''):
    unreal.unregister_slate_post_tick_callback(callback);perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    report.update(passed=passed,reason=reason,elapsed=time.monotonic()-state['started'])
    (out/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log('STICKY_BOMB '+json.dumps(report))
    if editor.get_game_world():levels.editor_request_end_play()
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if time.monotonic()-state['started']>120:raise RuntimeError('Timeout '+state['phase'])
        w=editor.get_game_world()
        if not w:return
        now=unreal.GameplayStatics.get_time_seconds(w);phase=state['phase']
        if phase=='setup':
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            halls=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)
            if not pc or not pawn or any(not h.get_editor_property('bReady') for h in halls):return
            state.update(w=w,pc=pc,pawn=pawn,tool=pawn.get_component_by_class(unreal.MiningBombToolComponent))
            movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent);movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            state['height']=pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
            state['floor']=cube('StickyFloor',base-unreal.Vector(0,0,20),unreal.Vector(100,100,.4))
            state.update(phase='case',at=now)
        elif phase=='case' and now-state['at']>.3:
            index=state['index'];name,pitch=cases[index]
            if valid(state.get('host')) and state['host']!=state['floor']:state['host'].destroy_actor()
            host=state['floor']
            if name in ('wall','moving-body'):
                host=cube('StickyWall',base+unreal.Vector(800,0,250),unreal.Vector(.3,10,5))
                if name=='moving-body':
                    host.static_mesh_component.set_collision_profile_name('PhysicsActor')
                    host.static_mesh_component.set_enable_gravity(False)
                    host.static_mesh_component.set_mass_override_in_kg('None',10000,True)
                    host.static_mesh_component.set_simulate_physics(True)
            elif name=='ceiling':host=cube('StickyCeiling',base+unreal.Vector(400,0,400),unreal.Vector(12,12,.4))
            state['pawn'].set_actor_location(base+unreal.Vector(0,0,state['height']+2),False,True)
            state['pc'].set_control_rotation(unreal.Rotator(pitch=pitch,yaw=0,roll=0))
            state['tool'].stow();press('G')
            state.update(host=host,name=name,case_start=now,phase='equip',at=now)
        elif phase=='equip' and now-state['at']>.15:
            check(state['tool'].get_editor_property('bEquipped'),state['name']+': G equips')
            press('LeftMouseButton');state.update(phase='flight',at=now)
        elif phase=='flight' and now-state['at']>.03:
            bomb=state['tool'].last_thrown_bomb;check(valid(bomb),state['name']+': real left click throws')
            check(bomb.collision.is_simulating_physics() and not bomb.get_editor_property('bStuck'),state['name']+': physical flight precedes contact')
            state.update(bomb=bomb,phase='contact',launch=bomb.launched_at)
        elif phase=='contact':
            bomb=state['bomb']
            if not valid(bomb):raise RuntimeError(state['name']+': bomb exploded without target contact')
            if not bomb.get_editor_property('bStuck'):return
            check(bomb.collision.get_attach_parent()==state['host'].static_mesh_component,state['name']+': sticks to intended collision surface')
            check(not bomb.collision.is_simulating_physics(),state['name']+': physics stops on first hit')
            check(bomb.stuck_at>=state['launch'] and bomb.stuck_at-state['launch']<3,state['name']+': sticks before fuse expires')
            state.update(position=bomb.get_actor_location(),relative=bomb.collision.get_editor_property('relative_location'),at=now,phase='hold')
            if state['name']=='moving-body':state['host'].static_mesh_component.set_physics_linear_velocity(unreal.Vector(0,200,0))
        elif phase=='hold' and now-state['at']>.45:
            bomb=state['bomb'];check(valid(bomb),state['name']+': remains attached before explosion')
            if state['name']=='moving-body':
                check(bomb.get_actor_location().distance(state['position'])>50,'moving-body: bomb follows real rigid-body movement')
                check(bomb.collision.get_editor_property('relative_location').distance(state['relative'])<.1,'moving-body: attachment offset stays fixed')
            else:check(bomb.get_actor_location().distance(state['position'])<.1,state['name']+': no bounce or gravity slide after contact')
            check(helper.capture_preview(state['pc'],str(out/(state['name']+'.png'))),'Captured '+state['name'])
            state.update(phase='fuse')
        elif phase=='fuse':
            bomb=state['bomb']
            if valid(bomb):state['last_position']=bomb.get_actor_location();return
            tool=state['tool'];check(tool.explosion_count==state['index']+1,state['name']+': exactly one explosion')
            fuse=tool.last_explosion_time-state['launch']
            check(abs(fuse-3)<.15,state['name']+': contact does not restart three-second throw fuse')
            check(tool.last_explosion_location.distance(state['last_position'])<35,state['name']+': explodes at attached surface location')
            report['cases'].append({'surface':state['name'],'fuse':fuse,'position':vec(tool.last_explosion_location)})
            state['index']+=1
            if state['index']==len(cases):finish(True)
            else:state.update(phase='case',at=now)
    except Exception:finish(False,traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world(),'Do not interrupt user preview'
callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
