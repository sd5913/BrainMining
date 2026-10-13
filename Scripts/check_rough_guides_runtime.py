"""PIE smoke check for the revised guide geometry, collision, excavation and reset."""
import unreal,json,time,traceback,bisect
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());OUT=ROOT/'Saved/Validation/main_game_rough_guides_runtime.json'
recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text())
STAGES=[('TunnelV1','BM_MainGame_V1'),('TunnelCavernV2','BM_MainGame_CavernV2'),('TunnelStalagmiteV3','BM_MainGame_StalagmiteV3'),('TunnelNeuralV4','BM_MainGame_NeuralV4')]
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);world=editor.get_game_world();assert world
pawn=unreal.GameplayStatics.get_player_character(world,0);assert pawn
headlamp=pawn.get_component_by_class(unreal.SpotLightComponent);assert headlamp and headlamp.is_visible() and headlamp.intensity>0
regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
assert sum(len(r.generated_guide_layers) for r in regions)==64
lights=[a.get_actor_label() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PointLight)]
assert not any(n.startswith(('MainGame_V2_','MainGame_V3_','MainGame_V4_')) for n in lights)
movement=pawn.get_component_by_class(unreal.CharacterMovementComponent);movement.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
state={'stage':0,'phase':0,'time':time.monotonic(),'started':time.monotonic()};result={'passed':False,'checks':[],'headlamp_visible':True,'auxiliary_cave_lights_removed':True}
def point(path,s):
    i=max(1,min(len(path['arc'])-1,bisect.bisect_left(path['arc'],s)));t=(s-path['arc'][i-1])/(path['arc'][i]-path['arc'][i-1])
    p=[a+(b-a)*t for a,b in zip(path['points'][i-1],path['points'][i])];return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def finish(ok,error=None):
    result.update(passed=ok,elapsed_seconds=time.monotonic()-state['started'])
    if error:result['error']=error
    OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROUGH_GUIDES_RUNTIME '+json.dumps(result,ensure_ascii=False))
    state['phase']=99
def tick(delta):
    try:
        now=time.monotonic()
        if state['phase']==99:unreal.unregister_slate_post_tick_callback(callback);return
        if not editor.get_game_world():finish(False,'PIE ended before verification');return
        if now-state['started']>45:raise AssertionError('Runtime smoke check timed out')
        if state['phase']==0:
            stage,tag=STAGES[state['stage']];r=next(r for r in regions if tag in [str(t) for t in r.tags]);guide=r.generated_guide_layers[0]
            path=json.loads((ROOT/'Art/MainGame'/stage/'centerlines.json').read_text())[0]
            record=json.loads((ROOT/'Art/MainGame'/stage/'placement_plan.json').read_text())['objects'][0];a=record['start_arc_m']
            origin=r.get_actor_location()+r.cutters[0].local_transform.translation
            position=point(path,a-1.8)+origin;target=point(path,a+1)+origin
            movement.stop_movement_immediately();pawn.set_actor_location(position,False,True)
            pawn.get_controller().set_control_rotation(unreal.MathLibrary.find_look_at_rotation(position+unreal.Vector(0,0,64),target))
            state.update(phase=1,time=now,target=target,guide=guide,initial=guide.triangle_count,stage_name=stage)
        elif state['phase']==1 and now-state['time']>.5:
            camera=unreal.GameplayStatics.get_player_camera_manager(world,0);eye=camera.get_camera_location()
            pawn.get_controller().set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,state['target']))
            hit=unreal.SystemLibrary.line_trace_single(world,eye,state['target'],unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9]==state['guide'],(state['stage_name'],'Rough front not hit',str(hit.to_tuple()[9] if hit else None))
            direction=state['target']-eye;direction=direction/direction.length();location=hit.to_tuple()[5]+direction*55
            assert state['guide'].mine_blast(location,110,.32,1701+state['stage']),state['guide'].last_status
            state.update(phase=2,time=now)
        elif state['phase']==2:
            guide=state['guide']
            if guide.successful_cuts>0:
                assert guide.triangle_count!=state['initial']
                material=guide.rock_material
                if recipe.get('guide_material'):assert material==unreal.load_asset(recipe['guide_material'])
                else:assert 'MI_Guide_' in material.get_name()
                cut_triangles=guide.triangle_count;guide.reset_rock()
                assert guide.successful_cuts==0 and guide.triangle_count==state['initial']
                assert all(c.mesh.get_material(0)==material for c in guide.chunks)
                result['checks'].append(dict(stage=state['stage_name'],collision_hits_guide=True,initial_triangles=state['initial'],cut_triangles=cut_triangles,reset_restores_geometry=True,material_persists=True))
                state['stage']+=1
                if state['stage']==len(STAGES):finish(True)
                else:state.update(phase=0,time=now)
            elif now-state['time']>9:raise AssertionError((state['stage_name'],'Excavation did not commit',guide.last_status))
    except Exception:
        if state.get('guide'):state['guide'].reset_rock()
        finish(False,traceback.format_exc())
callback=unreal.register_slate_post_tick_callback(tick)
