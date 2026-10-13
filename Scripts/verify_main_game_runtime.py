"""Isolated PIE check: real movement, TryMine, R reset, headlamp and deep descent safety."""
import unreal,json,time,traceback,bisect
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelV1'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
paths=json.loads((ART/'centerlines.json').read_text());records=json.loads((ART/'placement_plan.json').read_text())['objects']
report={'passed':False,'phase':'waiting'};state={'started':time.monotonic(),'phase':0}
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def point(s):
    p=paths[0];i=max(1,min(len(p['arc'])-1,bisect.bisect_left(p['arc'],s)));u=(s-p['arc'][i-1])/(p['arc'][i]-p['arc'][i-1])
    return raw([a+(b-a)*u for a,b in zip(p['points'][i-1],p['points'][i])])
def finish(passed,error=None):
    report.update(passed=passed,phase='complete' if passed else 'failed',elapsed_seconds=time.monotonic()-state['started'])
    if error:report['error']=error
    (ROOT/'Saved/Validation/main_game_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MAIN_GAME_RUNTIME '+json.dumps(report,ensure_ascii=False));levels.editor_request_end_play();state.update(phase=9,finished=time.monotonic())
def tick(delta):
    try:
        now=time.monotonic()
        if state['phase']==9:
            if editor.get_game_world() is None and now-state['finished']>1.:
                unreal.unregister_slate_post_tick_callback(callback);unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
            return
        if now-state['started']>100:finish(False,'Runtime test timed out');return
        if state['phase']==0 and now-state['started']>2:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');levels.editor_request_begin_play();state['phase']=1
        elif state['phase']==1 and editor.get_game_world():
            world=editor.get_game_world();pawn=unreal.GameplayStatics.get_player_character(world,0)
            if not pawn:return
            region=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)[0]
            mining=pawn.get_component_by_class(unreal.MiningComponent);assert pawn.get_class().get_name()=='BP_MainGameCharacter_C' and mining.fall_reset_distance==0
            light=pawn.get_component_by_class(unreal.SpotLightComponent);assert light.is_visible() and light.intensity>0
            state.update(world=world,pawn=pawn,region=region,mining=mining,spawn=pawn.get_actor_location(),time=now,
                         initial=region.generated_guide_layers[0].triangle_count,phase=2)
            report.update(character=pawn.get_class().get_name(),headlamp_visible=True,headlamp_intensity=light.intensity,fall_reset_distance=mining.fall_reset_distance)
        elif state['phase']==2:
            pawn=state['pawn']
            if now-state['time']<1.:pawn.add_movement_input(unreal.Vector(1,0,0),1.)
            else:
                distance=(pawn.get_actor_location()-state['spawn']).length();assert distance>60,distance
                report['actual_walk_cm']=distance;pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
                camera=unreal.GameplayStatics.get_player_camera_manager(state['world'],0)
                pawn.get_controller().set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_camera_location(),raw(records[0]['center_m'])))
                state.update(time=now,phase=3)
        elif state['phase']==3 and now-state['time']>.4:
            assert state['mining'].try_mine(),str(state['mining'].status);state.update(time=now,phase=4)
        elif state['phase']==4:
            guide=state['region'].generated_guide_layers[0]
            if guide.successful_cuts>0:
                report['try_mine_successful_cuts']=guide.successful_cuts;state['mining'].reset_field()
                assert guide.successful_cuts==0 and guide.triangle_count==state['initial'];report['manual_reset_restores_guides']=True
                state.update(time=now,phase=5)
            elif now-state['time']>8:raise AssertionError('TryMine did not commit a guide excavation: '+str(guide.last_status))
        elif state['phase']==5 and now-state['time']>.4:
            assert (state['pawn'].get_actor_location()-state['spawn']).length()<150
            main=[r for r in records if r['path_index']==0];candidates=[(a['end_arc_m']+b['start_arc_m'])*.5 for a,b in zip(main[:-1],main[1:]) if b['start_arc_m']-a['end_arc_m']>2.]
            s=next(s for s in candidates if point(s).z<state['spawn'].z-3000)
            center=point(s);hit=unreal.SystemLibrary.line_trace_single(state['world'],center,center-unreal.Vector(0,0,2000),unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,list(state['region'].generated_guide_layers),unreal.DrawDebugTrace.NONE,True)
            assert hit and hit.to_tuple()[9]==state['region'].generated_rock
            destination=hit.to_tuple()[5]+unreal.Vector(0,0,100)
            state['pawn'].get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately();state['pawn'].set_actor_location(destination,False,True)
            state.update(destination=destination,time=now,phase=6)
        elif state['phase']==6 and now-state['time']>1.2:
            pos=state['pawn'].get_actor_location();assert (pos-state['destination']).length()<150
            report['descent_without_reset_cm']=state['spawn'].z-pos.z;assert report['descent_without_reset_cm']>3000
            state['mining'].reset_field();finish(True)
    except Exception:finish(False,traceback.format_exc())
callback=unreal.register_slate_post_tick_callback(tick)
