"""Exercise a thrown bomb in the real area-0 cave, including debris and collision."""
import json,math,time,traceback
from pathlib import Path
import unreal
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper=unreal.MiningGUIEditorLibrary
out=Path(unreal.Paths.project_saved_dir())/'Validation/BombTerrain';out.mkdir(parents=True,exist_ok=True)
state={'phase':'setup','started':time.monotonic(),'busy':False};report={'passed':False,'checks':[]}
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def vec(v):return [v.x,v.y,v.z]
def check(v,label):
    assert v,label
    report['checks'].append(label)
def trace(a,b):return unreal.SystemLibrary.line_trace_single(state['w'],a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[state['pawn']],unreal.DrawDebugTrace.NONE,True)
def press(name):helper.send_preview_key(state['pc'],name,True);helper.send_preview_key(state['pc'],name,False)
def shot(name):check(helper.capture_preview(state['pc'],str(out/(name+'.png'))),'Captured '+name)
def finish(passed,reason=''):
    unreal.unregister_slate_post_tick_callback(callback);perf.set_editor_property('bThrottleCPUWhenNotForeground',old_throttle)
    report.update(passed=passed,reason=reason,elapsed=time.monotonic()-state['started'])
    (out/'runtime.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    unreal.log('BOMB_TERRAIN '+json.dumps(report))
    if editor.get_game_world():levels.editor_request_end_play()
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        if time.monotonic()-state['started']>150:raise RuntimeError('Timeout in '+state['phase'])
        phase=state['phase'];w=editor.get_game_world()
        if not w:return
        now=unreal.GameplayStatics.get_time_seconds(w)
        if phase=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property('bReady'):return
            pc=unreal.GameplayStatics.get_player_controller(w,0);pawn=unreal.GameplayStatics.get_player_pawn(w,0)
            check(halls[0].enter_area(0),'Entered actual Lvl_MainExperiences mining cave')
            movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            movement.set_movement_mode(unreal.MovementMode.MOVE_NONE)
            tool=pawn.get_component_by_class(unreal.MiningBombToolComponent)
            state.update(w=w,pc=pc,pawn=pawn,movement=movement,tool=tool,hall=halls[0],at=now,phase='aim')
        elif phase=='aim' and now-state['at']>.7:
            eye=state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()
            candidates=[]
            for i in range(80):
                a=2*math.pi*i/80;direction=unreal.Vector(math.cos(a),math.sin(a),.12)
                hit=trace(eye,eye+direction*4000)
                if not hit:continue
                h=hit.to_tuple()
                if isinstance(h[9],unreal.GeneratedMiningRock) and 'ExternalMiningGuideLayer' in [str(t) for t in h[9].tags] and h[3]>250:
                    candidates.append(h)
            check(bool(candidates),'Found a real cave blocker visible from the entry')
            h=min(candidates,key=lambda v:v[3]);point=h[5];normal=h[7]
            report['target']={'actor':h[9].get_name(),'point':vec(point),'normal':vec(normal),'material':h[10].get_material(0).get_path_name()}
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,point))
            rocks=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.GeneratedMiningRock)
            state.update(point=point,normal=normal,rocks=rocks,before={r.get_name():r.successful_cuts for r in rocks},at=now,phase='throw')
            shot('before')
        elif phase=='throw' and now-state['at']>.3:
            press('G');state.update(at=now,phase='equipped')
        elif phase=='equipped' and now-state['at']>.15:
            check(state['tool'].get_editor_property('bEquipped'),'G equips bomb in the actual cave')
            press('LeftMouseButton')
            state.update(at=now,phase='thrown')
        elif phase=='thrown' and now-state['at']>.15:
            check(state['tool'].last_thrown_bomb is not None,'Left click launches cave bomb')
            state.update(bomb=state['tool'].last_thrown_bomb,at=now,phase='fuse')
        elif phase=='fuse':
            tool=state['tool']
            if tool.explosion_count==0:return
            changed=[r for r in state['rocks'] if r.successful_cuts>state['before'][r.get_name()]]
            check(tool.last_excavated_rocks>0 and bool(changed),'Detonation excavates actual saved cave rocks')
            check(tool.last_debris_count==tool.settings.debris_count,'Explosion produces the configured 72 physical rock fragments')
            report.update(final_position=vec(tool.last_explosion_location),excavated_rocks=tool.last_excavated_rocks,debris_count=tool.last_debris_count,
                cuts=[{'actor':r.get_name(),'chunks':r.last_touched_chunks,'triangles':r.triangle_count,'milliseconds':r.last_total_milliseconds,'status':r.last_status} for r in changed])
            bursts=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningDebrisBurst)
            materials={b.source_material.get_path_name() for b in bursts if b.source_material}
            report['debris_materials']=sorted(materials)
            expected={r.rock_material.get_path_name() for r in changed if r.rock_material}
            check(bool(materials & expected),'Detached debris inherits a material from the excavated cave layers')
            effects=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningBombExplosion)
            check(len(effects)==1 and effects[0].effect.get_asset() is None,'Realistic mushroom-cloud system is detached')
            check(effects[0].fire.get_instance_count()>0 and effects[0].smoke.get_instance_count()>0,'Compact stylized fire and smoke are spawned')
            check(all(not e.get_editor_property('bHidden') for e in effects),'Experience-area visibility does not hide explosion effects')
            check(any(r.last_touched_chunks>1 for r in changed),'Actual cave blast updates multiple spatial chunks')
            state.update(changed=changed,at=now,phase='smoke')
            eye=state['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location()
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,tool.last_explosion_location))
            # Hide only the player's torso in this PIE capture, not tool components.
            for mesh in state['pawn'].get_components_by_class(unreal.SkeletalMeshComponent):mesh.set_visibility(False,False)
            state.update(phase='flash',at=now)
        elif phase=='flash' and now-state['at']>.07:
            shot('fire');state.update(phase='smoke',at=now)
        elif phase=='smoke' and now-state['at']>.5:
            shot('smoke');state.update(phase='crater',at=now)
        elif phase=='crater' and now-state['at']>3.5:
            shot('crater')
            # Compare unchanged and excavated collision in a fan through the last blast.
            center=state['tool'].last_explosion_location
            probes=[]
            for i in range(40):
                z=1-2*(i+.5)/40;a=i*2.39996322973;r=math.sqrt(1-z*z);direction=unreal.Vector(r*math.cos(a),r*math.sin(a),z)
                hit=trace(center,center+direction*750)
                if hit:probes.append(hit.to_tuple()[3])
            report['post_blast_collision_distances']=probes
            check(probes and max(probes)>300,'Collision traces reach the excavated cavity boundary')
            press('R');state.update(phase='reset',at=now)
        elif phase=='reset' and now-state['at']>.5:
            check(all(r.successful_cuts==0 for r in state['changed']),'R restores actual cave excavation')
            check(all(r.triangle_count==sum(c.initial_mesh.get_triangle_count() for c in r.chunks) for r in state['changed']),'Reset restores initial cave mesh triangle counts')
            shot('reset');finish(True)
    except Exception:finish(False,traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world(),'Do not interrupt a user preview'
callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
