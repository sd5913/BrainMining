"""Isolated PIE walk across both joints, mine/reset both areas, retain headlamp."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelCavernV2'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report={'passed':False,'phase':'waiting'};state={'started':time.monotonic(),'phase':0}
creation=json.loads((ROOT/'Saved/Validation/cavern_creation.json').read_text())
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(w,a,b,ignore):return unreal.SystemLibrary.line_trace_single(w,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,unreal.DrawDebugTrace.NONE,True)
def finish(ok,error=None):
    report.update(passed=ok,phase='complete' if ok else 'failed',elapsed_seconds=time.monotonic()-state['started'])
    if error:report['error']=error
    (ROOT/'Saved/Validation/cavern_runtime.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('CAVERN_RUNTIME '+json.dumps(report));levels.editor_request_end_play();state.update(phase=9,finished=time.monotonic())
def tick(delta):
    try:
        now=time.monotonic()
        if state['phase']==9:
            if editor.get_game_world() is None and now-state['finished']>1.:
                unreal.unregister_slate_post_tick_callback(callback);unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
            return
        if now-state['started']>90:finish(False,'Runtime test timed out');return
        if state['phase']==0 and now-state['started']>2:
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');levels.editor_request_begin_play();state['phase']=1
        elif state['phase']==1 and editor.get_game_world():
            w=editor.get_game_world();pawn=unreal.GameplayStatics.get_player_character(w,0)
            if not pawn:return
            regions=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.MiningExternalRegion)
            find=lambda tag:next(r for r in regions if tag in [str(t) for t in r.tags])
            old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')
            guides=[g for r in regions for g in r.generated_guide_layers]
            origin=old.get_actor_location()+old.cutters[0].local_transform.translation
            p=origin+unreal.Vector(6130,0,1400);hit=trace(w,p,p-unreal.Vector(0,0,1600),guides)
            assert hit and hit.to_tuple()[0]
            start=hit.to_tuple()[5]+unreal.Vector(0,0,100)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
            pawn.set_actor_location(start,False,True)
            pawn.get_controller().set_control_rotation(unreal.Rotator(yaw=0))
            mining=pawn.get_component_by_class(unreal.MiningComponent);assert mining.fall_reset_distance==0
            light=pawn.get_component_by_class(unreal.SpotLightComponent);assert light.is_visible() and light.intensity>0
            state.update(world=w,pawn=pawn,mining=mining,old=old,new=new,reserve=reserve,origin=origin,start=start,time=now,phase=2)
            report.update(headlamp_visible=True,region_count=len(regions),start_cm=[start.x,start.y,start.z])
        elif state['phase']==2:
            pawn=state['pawn'];pos=pawn.get_actor_location();target=state['origin'].x+8640
            if pos.x<target and now-state['time']<12:
                pawn.add_movement_input(unreal.Vector(1,0,0),1.)
            else:
                report.update(walk_distance_cm=pos.x-state['start'].x,end_cm=[pos.x,pos.y,pos.z])
                assert pos.x>=target,('Cannot walk through reserved connector',report)
                assert abs(pos.y-state['start'].y)<200 and pos.z>state['start'].z-1200,'Fell outside region'
                pawn.get_component_by_class(unreal.CharacterMovementComponent).stop_movement_immediately()
                origin=unreal.Vector(*creation['new_origin_cm'])
                record=json.loads((ART/'placement_plan.json').read_text())['objects'][0]
                newguide=state['new'].generated_guide_layers[0];oldguide=state['old'].generated_guide_layers[0]
                assert newguide.mine_sphere(raw(record['center_m'])+origin,120),str(newguide.last_status)
                oldrecord=json.loads((ROOT/'Art/MainGame/TunnelV1/placement_plan.json').read_text())['objects'][0]
                assert oldguide.mine_sphere(raw(oldrecord['center_m'])+state['origin'],120)
                assert newguide.successful_cuts==1 and oldguide.successful_cuts==1
                state['mining'].reset_field();assert newguide.successful_cuts==0 and oldguide.successful_cuts==0
                report.update(both_regions_excavation_and_R_reset=True,walked_across_both_shell_seams=True)
                spec=json.loads((ART/'pillars.json').read_text())[0]
                destination=raw(spec['center_m'])+origin+unreal.Vector(0,250,0)
                ignore=list(state['new'].generated_guide_layers)
                hit=trace(state['world'],destination,destination-unreal.Vector(0,0,2000),ignore)
                assert hit and hit.to_tuple()[9]==state['new'].generated_rock
                destination=hit.to_tuple()[5]+unreal.Vector(0,0,100)
                pawn.set_actor_location(destination,False,True);state.update(destination=destination,time=now,phase=3)
        elif state['phase']==3 and now-state['time']>1.5:
            pos=state['pawn'].get_actor_location();assert (pos-state['destination']).length()<150,'Unexpected teleport/fall in cavern'
            report.update(cavern_stays_after_deep_descent=True,cavern_position_cm=[pos.x,pos.y,pos.z])
            state['mining'].reset_field();finish(True)
    except Exception:finish(False,traceback.format_exc())
callback=unreal.register_slate_post_tick_callback(tick)
