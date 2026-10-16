"""Real bomb recharge HUD, jetpack transfers and saved mineral rewards in PIE."""
import unreal,json,time,traceback,sys,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV6';sys.path.insert(0,str(ROOT/'Scripts'))
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
perf=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));old=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
s={'phase':'setup','next':0,'started':time.monotonic(),'pickup':0,'busy':False};report={'passed':False,'checks':[],'recharge':[],'transfers':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
def key(n,on):helper.send_preview_key(s['pc'],n,on)
def aim(p):s['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location(),p))
def move(p,fall=False):
    s['pawn'].set_actor_location(p,False,True);s['movement'].stop_movement_immediately();s['movement'].set_movement_mode(unreal.MovementMode.MOVE_FALLING if fall else unreal.MovementMode.MOVE_FLYING)
def enter(i):
    s['hall'].set_editor_property('linear_campaign',False)
    try:assert s['hall'].enter_area(i)
    finally:s['hall'].set_editor_property('linear_campaign',True)
def shot(name):check(helper.capture_preview(s['pc'],str(OUT/(name+'.png'))),'Screenshot '+name)
def trace(a,b,ignored=None):
    result=unreal.SystemLibrary.line_trace_single(s['world'],a,b,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored or [s['pawn']],unreal.DrawDebugTrace.NONE,True)
    return result.to_tuple() if result else None
def finish(error=None):
    if 'pc' in s:
        key('E',False);key('W',False);key('SpaceBar',False);s['movement'].set_flight_input_held(False)
    report.update(passed=error is None,error=error,phase=s['phase'],elapsed=time.monotonic()-s['started']);(OUT/'presentation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');perf.set_editor_property('bThrottleCPUWhenNotForeground',old);levels.editor_request_end_play();s['phase']='done'
def tick(dt):
    if s['busy']:return
    if s['phase']=='done':
        if not ed.get_game_world():unreal.unregister_slate_post_tick_callback(callback);unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
        return
    s['busy']=True
    try:
        now=time.monotonic();assert now-s['started']<420,'Presentation timeout'
        if now<s['next']:return
        world=ed.get_game_world()
        if not world:return
        phase=s['phase']
        if phase=='setup':
            pc=unreal.GameplayStatics.get_player_controller(world,0);halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            # Slate ticks also run inside resource compilation dialogs while
            # the game world and camera are still stopped at their first frame.
            if not pc or not pc.mining_gui or not halls or not halls[0].get_editor_property('bReady') or not pc.mining_gui.get_editor_property('bMineralProgression') or unreal.GameplayStatics.get_time_seconds(world)<2:return
            pawn=unreal.GameplayStatics.get_player_pawn(world,0);s.update(world=world,pc=pc,pawn=pawn,hall=halls[0],movement=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent),bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent),pickups=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStarterPickup)),room=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningStartRoom)[0],phase='pickup_aim',next=now+2)
            halls[0].set_actor_tick_enabled(False)
        elif phase=='pickup_aim':
            p=s['pickups'][s['pickup']].get_actor_location();move(unreal.Vector(p.x-125,p.y,s['room'].get_actor_location().z+102));aim(p);s.update(phase='pickup_press',next=now+.5)
        elif phase=='pickup_press':
            aim(s['pickups'][s['pickup']].get_actor_location())
            report['pickup_context']=dict(index=s['pickup'],target=str(s['room'].get_target(s['pc'])),cursor=s['pc'].get_editor_property('bShowMouseCursor'),position=list(s['pawn'].get_actor_location().to_tuple()),pickup=list(s['pickups'][s['pickup']].get_actor_location().to_tuple()))
            report['pickup_context'].update(view_target=str(s['pc'].get_view_target()),paused=unreal.GameplayStatics.is_game_paused(world),camera=list(s['pc'].player_camera_manager.get_camera_location().to_tuple()),rotation=list(s['pc'].get_control_rotation().to_tuple()),component_camera=list(s['pawn'].get_component_by_class(unreal.CameraComponent).get_world_location().to_tuple()),pawn_tick=s['pawn'].is_actor_tick_enabled(),controller_tick=s['pc'].is_actor_tick_enabled(),mineral_gui=s['pc'].mining_gui.get_editor_property('bMineralProgression'))
            key('E',True);s.update(phase='pickup_check',next=now+.15)
        elif phase=='pickup_check':
            key('E',False)
            if not s['pickups'][s['pickup']].collected:
                s['attempt']=s.get('attempt',0)+1
                if s['attempt']==4:shot('pickup_diagnostic')
                assert s['attempt']<5;s.update(phase='pickup_aim',next=now+1);return
            s['attempt']=0;s['pickup']+=1;s.update(phase='pickup_aim' if s['pickup']<len(s['pickups']) else 'chamber',next=now+.3)
        elif phase=='chamber':
            enter(2);features=json.loads((ROOT/'Art/MainGame/RoutesV6/TunnelCavernV2/features.json').read_text());s['features']=features['platforms'];s['origin']=unreal.Vector(-7700,0,400)
            raw=lambda p:s['origin']+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
            s['tops']=[raw(p['top_m']) for p in s['features']];move(raw([-37,-6,69]));aim(s['tops'][2]);s.update(phase='bomb_ready',next=now+1)
        elif phase=='bomb_ready':
            check(s['bomb'].unlock_skill(),'Bomb skill unlocked');check(s['bomb'].max_bombs==1,'Capacity remains one');shot('parkour_mushrooms');shot('bomb_ready')
            check(s['bomb'].toggle_equipped() and s['bomb'].throw_bomb(),'Throw the actual bomb');check(s['bomb'].bomb_count==0 and not s['bomb'].throw_bomb(),'Cannot throw another while cooling')
            thrown=s['bomb'].last_thrown_bomb;thrown.collision.set_simulate_physics(False);thrown.set_actor_location(unreal.Vector(0,0,-50000),False,True)
            s.update(phase='cooldown',capture=0,next=now+.02)
        elif phase=='cooldown':
            bomb=s['bomb'];ui=s['pc'].mining_gui.ability_g
            if bomb.bomb_count==0:
                check(ui.cooldown_text.get_visibility()!=unreal.SlateVisibility.VISIBLE and not str(ui.cooldown_text.get_text()),'No capacity/countdown text') if s['capture']==0 and bomb.recharge_progress>.1 else None
                if s['capture']<3 and bomb.recharge_progress>=[.12,.5,.85][s['capture']]:
                    check(ui.get_editor_property('bUsed') and ui.ability_icon.color_and_opacity.g<.3,'Bomb icon red during recharge')
                    check(ui.recharge_bar.bar_fill_type==unreal.ProgressBarFillType.BOTTOM_TO_TOP,'Recharge fills upward')
                    check(abs(ui.recharge_bar.percent-bomb.recharge_progress)<.08,'HUD follows the actual inventory timer')
                    report['recharge'].append(dict(progress=bomb.recharge_progress,bar=ui.recharge_bar.percent));shot('bomb_cooldown_'+str(s['capture']));s['capture']+=1
            else:
                check(s['capture']==3 and bomb.bomb_count==1,'Recharges exactly one bomb');check(not ui.get_editor_property('bUsed') and ui.recharge_bar.get_visibility()==unreal.SlateVisibility.COLLAPSED,'Ready state clears the red cooldown bar');shot('bomb_recharged');bomb.stow();s.update(phase='transfer_setup',transfer=0,next=now+.1)
        elif phase=='transfer_setup':
            # Test the high transitions in both chambers with the actual jetpack.
            pairs=[(0,1),(1,2),(2,3),(3,4),(5,6),(6,7),(7,8),(8,9)];s['pair']=pairs[s['transfer']];a,b=s['pair'];move(s['tops'][a]+unreal.Vector(0,0,100),fall=True);aim(s['tops'][b]+unreal.Vector(0,0,100));s.update(phase='transfer_settle',deadline=now+10,next=now+.3)
        elif phase=='transfer_settle':
            if not s['movement'].is_moving_on_ground() or s['movement'].jetpack_heat>.02:
                assert now<s['deadline'],'Cannot settle/cool on mushroom platform';return
            s['movement'].set_flight_input_held(True);s.update(phase='transfer_fly',deadline=now+12)
        elif phase=='transfer_fly':
            a,b=s['pair'];target=s['tops'][b]+unreal.Vector(0,0,100);pos=s['pawn'].get_actor_location();delta=target-pos;horizontal=unreal.Vector(delta.x,delta.y,0)
            assert now<s['deadline'],('Jetpack transfer timed out',a,b,pos,target)
            if pos.z>target.z+80:s['crossing']=True
            if s.get('crossing'):
                if horizontal.length()>55:s['pawn'].add_movement_input(horizontal.normal(),min(1,horizontal.length()/160))
                s['movement'].set_flight_input_held(pos.z<target.z+130 and horizontal.length()>100)
                if horizontal.length()<100:s['movement'].set_flight_input_held(False)
                if s['movement'].is_moving_on_ground():
                    h=trace(pos,pos-unreal.Vector(0,0,160));check(h and h[0] and h[9].get_actor_label().endswith(s['features'][b]['name']),'Real jetpack lands on '+s['features'][b]['name']);report['transfers'].append(dict(start=a,end=b,rise=s['tops'][b].z-s['tops'][a].z));s['transfer']+=1;s['crossing']=False;s.update(phase='transfer_setup' if s['transfer']<8 else 'rewards',next=now+.1)
        elif phase=='rewards':
            author=json.loads((OUT/'authoring.json').read_text());items=list(s['hall'].treasure.items)
            check(len(items)==101,'Sparse mineral layout loaded')
            for reward in author['minerals']['rewards']:
                item=min(items,key=lambda a:a.get_actor_location().distance(unreal.Vector(*reward['core'])))
                check(str(item.treasure_id)==str(reward['mineral']),'Top-tier reward in '+str(reward['region'])+'/'+reward['branch'])
            s.update(phase='reward_photos',photo=0)
        elif phase=='reward_photos':
            author=json.loads((OUT/'authoring.json').read_text());reward=author['minerals']['rewards'][s['photo']*4];enter(s['photo']*2)
            p=unreal.Vector(*reward['surface']);core=unreal.Vector(*reward['core']);normal=(p-core).normal();move(p+normal*350-unreal.Vector(0,0,64));aim(p);s.update(phase='reward_shot',next=now+1)
        elif phase=='reward_shot':
            shot('dead_end_'+str(s['photo']+1));s['photo']+=1
            if s['photo']<3:s['phase']='reward_photos'
            else:finish()
    except Exception:finish(traceback.format_exc())
    finally:s['busy']=False
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');unreal.EditorPythonScripting.set_keep_python_script_alive(True);callback=unreal.register_slate_post_tick_callback(tick);helper.start_floating_preview()
