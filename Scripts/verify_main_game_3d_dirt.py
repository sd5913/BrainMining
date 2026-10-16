"""Verify saved geometry relief, excavation and reset, with oblique PIE images."""
# Reuse the existing PIE fixture/input helpers; its tick callback is not loaded.
from pathlib import Path
import unreal,json,time,traceback,sys
sys.path.insert(0,str(Path(unreal.Paths.project_dir()).resolve()/'Scripts'))
from main_game_route_sources import tuning
ROOT=Path(unreal.Paths.project_dir()).resolve()
fixture=(ROOT/'Scripts/verify_main_game_dirt_transition.py').read_text(encoding='utf-8').split('\ndef tick(dt):')[0]
exec(compile(fixture,'dirt_pie_fixture','exec'),globals())
OUT=ROOT/'Saved/Validation/MainGameDirt3D'
if (ROOT/'Config/MainGameRoutes.json').exists():
    OUT=ROOT/'Saved/Validation/MainGameRoutesV2/Clay'
bake=json.loads((OUT/'bake.json').read_text(encoding='utf-8'));assert bake['passed']
report={'passed':False,'checks':[],'projections_cm':[]}
state.update(index=0,pickup_index=0)
def V(p):return unreal.Vector(*p)
def protrusion(rec):
    anchor=V(rec['position']);normal=V(rec['normal']);depth=rec['depth_cm']
    side=unreal.Vector(-normal.y,normal.x,0);side=side/max(side.length(),.001)*2
    # Avoid a ray passing exactly through the sphere's shared pole vertex.
    h=unreal.SystemLibrary.line_trace_single(state['world'],anchor+side+normal*(depth+60),anchor+side-normal*30,
        unreal.TraceTypeQuery.ECC_VISIBILITY,True,[state['pawn']]+state['ignored_guides'],unreal.DrawDebugTrace.NONE,True)
    if not h or not h.to_tuple()[0]:return None,None
    h=h.to_tuple();return unreal.MathLibrary.dot_vector_vector(h[5]-anchor,normal),h[9]
def tick(dt):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done' or now<state['next']:return
        assert now-state['start']<180,'3D soil verification timed out'
        world=editor.get_game_world()
        if not world:return
        phase=state['phase']
        if phase=='setup':
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
            rooms=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)
            if not pawn or not rooms or not pc.mining_gui:return
            regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion))
            state.update(world=world,pawn=pawn,pc=pc,room=rooms[0],movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),
                pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),regions=regions,
                ignored_guides=[g for r in regions for g in r.generated_guide_layers])
            check(all(c.get_editor_property('hidden_in_game') for c in pawn.get_components_by_class(unreal.SkeletalMeshComponent)),'Player body remains hidden')
            check(len(next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningTreasureField)).items)==tuning(ROOT)['expected_deposits'],'All configured mineral deposits remain present')
            for item in bake['stages']:
                r=next(r for r in regions if r.actor_has_tag(item['tag']))
                check(r.generated_rock.triangle_count==item['after_triangles'],item['stage']+' reload preserves the baked 3D geometry')
            state.update(phase='aim_pickup',next=now+.2)
        elif phase=='aim_pickup':
            p=state['pickups'][state['pickup_index']].get_actor_location()
            move(unreal.Vector(p.x-125,p.y,state['room'].get_actor_location().z+102));look(p)
            state.update(phase='collect',next=now+.25)
        elif phase=='collect':
            press('E');check(state['pickups'][state['pickup_index']].collected,'Starter pickup still works')
            state['pickup_index']+=1
            state.update(phase='aim_pickup' if state['pickup_index']<2 else 'stage',next=now+.25)
        elif phase=='stage':
            if state['index']<4:
                stage,tag=STAGES[state['index']]
                host=next(r.generated_rock for r in state['regions'] if r.actor_has_tag(tag))
                rec=next(r for r in bake['samples'] if r['kind']==tag and abs(r['normal'][2])<.4 and r.get('previous_depth_cm',r['depth_cm'])>10)
            else:
                stage='StartRoomJunction'
                host=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GeneratedMiningRock) if a.actor_has_tag('BM_MainGame_3DDirtJunction'))
                rec=next(r for r in bake['samples'] if r['kind']=='junction' and abs(r['normal'][2])<.4 and r.get('previous_depth_cm',r['depth_cm'])>9)
            anchor=V(rec['position']);normal=V(rec['normal']);tangent=unreal.Vector(-normal.y,normal.x,0)
            state.update(host=host,rec=rec,stage_name=stage,initial=host.triangle_count,anchor=anchor,normal=normal,tangent=tangent)
            move(anchor+normal*185+tangent*65-unreal.Vector(0,0,64))
            state['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            look(anchor);state.update(phase='measure',next=now+.7)
        elif phase=='measure':
            d,hit_actor=protrusion(state['rec'])
            check(d is not None and d>3 and hit_actor==state['host'],state['stage_name']+' has actual collision geometry protruding beyond the old wall: '+str(d))
            report['projections_cm'].append(dict(stage=state['stage_name'],height_cm=d))
            shot('%02d_%s_solid_clods'%(state['index']+1,state['stage_name']))
            state.update(phase='orbit',next=now+.15)
        elif phase=='orbit':
            move(state['anchor']+state['normal']*130-state['tangent']*100-unreal.Vector(0,0,64))
            look(state['anchor']);state.update(phase='cut',next=now+.45)
        elif phase=='cut':
            shot('%02d_%s_side_profile'%(state['index']+1,state['stage_name']))
            radius=state['rec']['width_radius_cm']+35
            check(state['host'].mine_blast(state['anchor']+state['normal']*3,radius,.08,9300+state['index']),state['stage_name']+' 3D clod accepts mining')
            state.update(phase='removed',next=now+.35)
        elif phase=='removed':
            if not state['host'].successful_cuts:return
            d,a=protrusion(state['rec'])
            if state['index']==4:
                check(d is not None and d<1 and isinstance(a,unreal.StaticMeshActor),'Entrance clay mines away while its unmineable supporting wall remains')
            else:
                check(d is None or d<0,state['stage_name']+' mining removes the solid clod and its underlying support together')
            shot('%02d_%s_excavated_clod'%(state['index']+1,state['stage_name']))
            state['host'].reset_rock();state.update(phase='restored',next=now+.25)
        elif phase=='restored':
            d,a=protrusion(state['rec'])
            check(d is not None and d>3 and a==state['host'],state['stage_name']+' reset restores the real 3D clod')
            check(state['host'].triangle_count==state['initial'],state['stage_name']+' reset restores the saved triangle count')
            state['index']+=1
            if state['index']<5:state.update(phase='stage',next=now+.2)
            else:finish()
    except Exception:finish(traceback.format_exc())
    finally:state['busy']=False
assert not editor.get_game_world()
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
callback=unreal.register_slate_post_tick_callback(tick)
helper.start_floating_preview()
