"""Actual multi-layer bomb crater, seam rays and starting shaft collision in PIE."""
import unreal,json,time,traceback,sys,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV6'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':'setup','next':0,'start':time.monotonic(),'busy':False};report={'passed':False,'checks':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def trace(a,b,ignore):
    h=unreal.SystemLibrary.line_trace_single(s['world'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
def move(p,target):
    s['pawn'].set_actor_location(p,False,True);s['movement'].stop_movement_immediately();s['movement'].set_movement_mode(unreal.MovementMode.MOVE_FLYING)
    s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),target))
def shot(name):check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Screenshot '+name)
def finish(error=None):
    report.update(passed=error is None,error=error,phase=s['phase']);(OUT/'blast-seams.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');perf.set_editor_property('bThrottleCPUWhenNotForeground',old);levels.editor_request_end_play();s.update(phase='done',quit_after=time.monotonic()+3)
def tick(dt):
    if s['busy']:return
    if s['phase']=='done':
        if time.monotonic()>s['quit_after'] and not ed.get_game_world():unreal.unregister_slate_post_tick_callback(callback);unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
        return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['start']<240
        if now<s['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=s['phase']
        if phase=='setup':
            pc=unreal.GameplayStatics.get_player_controller(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not pc or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui.get_editor_property('bMineralProgression') or unreal.GameplayStatics.get_time_seconds(world)<2:return
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);hall=halls[0];hall.set_actor_tick_enabled(False)
            s.update(world=world,pc=pc,pawn=pawn,hall=hall,movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent))
            s.update(pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0],pickup=0,phase='pickup_aim',next=now+.2)
        elif phase=='pickup_aim':
            p=s['pickups'][s['pickup']].get_actor_location();move(unreal.Vector(p.x-125,p.y,s['room'].get_actor_location().z+102),p);s.update(phase='pickup_press',next=now+.6)
        elif phase=='pickup_press':helper.send_preview_key(s['pc'],'E',True);s.update(phase='pickup_check',next=now+.15)
        elif phase=='pickup_check':
            helper.send_preview_key(s['pc'],'E',False);check(s['pickups'][s['pickup']].collected,'Starter tool collected');s['pickup']+=1;s.update(phase='pickup_aim' if s['pickup']<len(s['pickups']) else 'shaft_prepare',next=now+.3)
        elif phase=='shaft_prepare':
            move(unreal.Vector(-32700,0,11250),unreal.Vector(-32700,0,12100));s.update(phase='shaft',next=now+1)
        elif phase=='shaft':
            a=unreal.Vector(-32700,0,11250);b=unreal.Vector(-32700,0,12010);c=unreal.Vector(-31200,0,12010)
            for start,end in [(a,b),(b,c)]:
                h=unreal.SystemLibrary.capsule_trace_single(world,start,end,42,90,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[s['pawn']],unreal.DrawDebugTrace.NONE,True)
                check(not h or not h.to_tuple()[0],'Player capsule clears starting shaft and upper exit')
            shot('starting_shaft');s.update(phase='find_seam')
        elif phase=='find_seam':
            s['hall'].set_editor_property('linear_campaign',False);assert s['hall'].enter_area(0);s['hall'].set_editor_property('linear_campaign',True)
            region=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion) if a.actor_has_tag('BM_Campaign_MainGame01'))
            paths=json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelV1/centerlines.json').read_text());plan=json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelV1/placement_plan.json').read_text())['objects'][0];path=paths[plan['path_index']];origin=region.get_actor_location()+region.cutters[0].local_transform.translation
            raw=lambda p:origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
            index=min(range(len(path['arc'])),key=lambda j:abs(path['arc'][j]-plan['start_arc_m']))
            wall=region.generated_rock;guide=region.generated_guide_layers[0];rocks=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DiggableRock));ignored=[s['pawn'],*s['hall'].treasure.items];samples=[]
            for j in range(max(1,index-12),min(index+5,len(path['points'])-1),2):
                p=raw(path['points'][j]);t=(raw(path['points'][j+1])-p).normal();side=t.cross(unreal.Vector(0,0,1)).normal()
                for k in range(12):
                    d=side*math.cos(k*math.tau/12)+unreal.Vector(0,0,1)*math.sin(k*math.tau/12);h=trace(p,p+d*2000,ignored)
                    if h and h[0] and h[9] in [wall,guide]:samples.append(h)
            pairs=[(a,b) for a in samples if a[9]==wall for b in samples if b[9]==guide]
            check(bool(pairs),'Both material surfaces visible beside the blockage')
            a,b=min(pairs,key=lambda p:p[0][5].distance(p[1][5]));check(a[5].distance(b[5])<350,'Find a real shared clay/rock edge within bomb diameter')
            center=(a[5]+b[5])/2+(a[6]+b[6]).normal()*45;normal=(a[6]+b[6]).normal()
            s.update(center=center,normal=normal,wall=wall,guide=guide,rocks=rocks,before=[wall.successful_cuts,guide.successful_cuts])
            move(center+normal*550-unreal.Vector(0,0,64),center);s.update(phase='throw',next=now+1)
        elif phase=='throw':
            shot('blast_seam_before');bomb=s['bomb'];check(bomb.unlock_skill(),'Bomb unlocked for regression');check(bomb.toggle_equipped() and bomb.throw_bomb(),'Actual bomb thrown at the two-material seam');thrown=bomb.last_thrown_bomb;thrown.collision.set_simulate_physics(False);thrown.set_actor_location(s['center'],False,True);thrown.explode();s.update(phase='crater',next=now+2,deadline=now+30)
        elif phase=='crater':
            if any(a.get_editor_property('bCutPending') for a in [s['wall'],s['guide']]):assert now<s['deadline'];return
            check(s['wall'].successful_cuts>s['before'][0] and s['guide'].successful_cuts>s['before'][1],'One real explosion cuts both clay and wall')
            overlap=[];shared=0
            for k in range(256):
                z=1-2*(k+.5)/256;r=math.sqrt(1-z*z);d=unreal.Vector(r*math.cos(k*2.39996323),r*math.sin(k*2.39996323),z);hits=[]
                for target in [s['wall'],s['guide']]:
                    ignored=[s['pawn'],*s['hall'].treasure.items,*[a for a in s['rocks'] if a!=target]]
                    h=trace(s['center'],s['center']+d*600,ignored);hits.append(h if h and h[0] and h[9]==target else None)
                if all(hits):
                    shared+=1
                    if hits[0][5].distance(hits[1][5])<.25:overlap.append(k)
            report.update(shared_ray_directions=shared,coincident_surfaces=overlap,blast_location=list(s['center'].to_tuple()))
            check(not overlap,'No coincident crater faces along 256 sampled directions');shot('blast_seam_after');finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'bm.Mining.LazyRuntimeChunks 0');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
