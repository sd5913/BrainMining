"""Exercise real MainGame mineral excavation/pickup, UI purchases, abilities and area travel."""
import unreal,json,time,traceback,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameSystems';OUT.mkdir(parents=True,exist_ok=True)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);helper=unreal.MiningGUIEditorLibrary
result={'passed':False,'checks':[]};state={'phase':'setup','started':time.monotonic(),'next':0,'busy':False}
def check(ok,text):
    assert ok,text;result['checks'].append(text)
def key(name,down):helper.send_preview_key(state['pc'],name,down)
def capture(name):check(helper.capture_preview(state['pc'],str(OUT/(name+'.png'))),'Captured '+name)
def finish(ok,error=None):
    result.update(passed=ok,elapsed_seconds=time.monotonic()-state['started'])
    if error:result['error']=error
    (OUT/'runtime.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_SYSTEMS_RUNTIME '+json.dumps(result))
    if state.get('pc'):
        for name in ['Tab','SpaceBar','LeftControl','LeftMouseButton']:key(name,False)
    state['phase']='done'
def tick(delta):
    if state['busy']:return
    state['busy']=True
    try:
        now=time.monotonic()
        if state['phase']=='done':unreal.unregister_slate_post_tick_callback(callback);return
        if now-state['started']>150:raise AssertionError('Runtime verification timed out: '+state['phase'])
        if now<state['next']:return
        world=editor.get_game_world()
        if not world:raise AssertionError('PIE stopped')
        phase=state['phase']
        if phase=='setup':
            halls=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property('bReady'):return
            hall=halls[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0);gui=pc.mining_gui
            if not gui or not gui.drill_upgrade:return
            progression=pawn.get_component_by_class(unreal.MiningProgressionComponent);wallet=progression.get_wallet()
            mining=pawn.get_component_by_class(unreal.MiningComponent);flight=pawn.get_component_by_class(unreal.MiningCharacterMovementComponent);bomb=pawn.get_component_by_class(unreal.MiningBombToolComponent)
            field=hall.treasure;manifest=json.loads((ROOT/'Saved/Validation/main_game_system_integration.json').read_text())
            check(pawn.get_class().get_name()=='BP_MainGameCharacter_C','Uses MainGame character')
            check(progression.initialized and mining.use_drill_energy and bomb.use_inventory,'Existing progression enables drill energy and bomb inventory')
            check(flight.flight_settings.enabled and flight.flight_settings.enable_heat,'Jetpack and heat system enabled')
            check(bomb.is_skill_unlocked(),'Bomb is usable in this mining-only level')
            check(len(field.items)==manifest['deposits'] and len(hall.areas)==4,'All authored deposits are loaded across four areas')
            check(wallet.balance==0 and all(progression.get_level(i)==0 for i in range(4)),'Fresh run starts at zero coins and upgrades')
            check(not hall.purchase_upgrade(0,1) and wallet.balance==0,'Cannot purchase without coins')
            check(not hall.purchase_upgrade(1,2),'Cannot skip upgrade levels')
            placement=field.get_editor_property('authored_placements')[0]
            item=next(i for i in field.items if i.actor_has_tag(str(placement.area_tag)) and i.get_actor_location()==placement.transform.translation)
            clue_count=item.get_editor_property('CrystalClues').get_instance_count();check(clue_count>20,'Surface and internal crystal clues are instantiated')
            state.update(world=world,hall=hall,pc=pc,pawn=pawn,gui=gui,progression=progression,wallet=wallet,mining=mining,flight=flight,bomb=bomb,field=field,item=item,
                placement=placement,clue_count=clue_count,initial_coins=wallet.balance,
                cards=[gui.pickaxe_upgrade,gui.vortex_upgrade,gui.blast_upgrade,gui.drill_upgrade],radius=mining.radius,interval=mining.interval,
                heat=flight.flight_settings.heat_up_seconds,blast=bomb.settings.excavation_radius,prices=[[s.price for s in t.steps] for t in progression.tracks])
            flight.set_movement_mode(unreal.MovementMode.MOVE_FLYING)
            flight.set_editor_property('gravity_scale',0.)
            n=unreal.MathLibrary.get_forward_vector(placement.surface.rotation.rotator());eye=placement.surface.translation+n*180
            pawn.set_actor_location(eye-unreal.Vector(0,0,64),False,True);pc.set_control_rotation(unreal.MathLibrary.find_look_at_rotation(eye,placement.surface.translation))
            state.update(phase='clues',next=now+.8)
        elif phase=='clues':
            capture('01_crystal_clues')
            for host in state['placement'].hosts:check(host.mine_blast(state['item'].get_actor_location(),500,.2,841),'Excavated a covering rock')
            state.update(phase='expose',next=now+.6)
        elif phase=='expose':
            item=state['item']
            if item.exposure<=.5:
                state['next']=now+.25;return
            check(item.exposure>.5,'Mineral releases after sufficient real excavation')
            check(item.released,'Excavated mineral becomes a physical drop')
            check(item.get_editor_property('CrystalClues').get_instance_count()<state['clue_count'],'Unsupported fragments disappear after excavation')
            item.drop_body.set_simulate_physics(False)
            position=item.get_actor_location();state['pawn'].set_actor_location(position+unreal.Vector(0,-180,-64),False,True)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(position+unreal.Vector(0,-180,0),position))
            state.update(phase='pickup',next=now+.4)
        elif phase=='pickup':
            item=state['item'];camera=unreal.GameplayStatics.get_player_camera_manager(world,0)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_camera_location(),item.get_actor_location()))
            state.update(phase='pickup_key',next=now+.3)
        elif phase=='pickup_key':
            key('E',True);key('E',False);state.update(phase='reward',next=now+.4)
        elif phase=='reward':
            check(state['item'].collected,'E collects the excavated mineral')
            check(state['wallet'].balance==state['initial_coins']+state['item'].value,'Pickup credits shared wallet exactly once')
            state['collected_balance']=state['wallet'].balance;key('E',True);key('E',False);state.update(phase='panel',next=now+.4)
        elif phase=='panel':
            check(state['wallet'].balance==state['collected_balance'],'Repeated E does not duplicate coins')
            state['wallet'].add_coins(5000000);key('Tab',True);state.update(phase='buy',track=0,level=0,next=now+1.2)
        elif phase=='buy':
            track=state['track'];level=state['level'];card=state['cards'][track]
            if track==0 and level==0:capture('02_upgrade_panel')
            before=state['wallet'].balance
            check(helper.simulate_preview_button(card.coin_button,True),'Upgrade button pressed');check(helper.simulate_preview_button(card.coin_button,False),'Upgrade button released')
            check(state['progression'].get_level(track)==level+1,'UI purchase advances track '+str(track))
            check(state['wallet'].balance==before-state['prices'][track][level],'UI purchase deducts exact price')
            if level==3:track+=1;level=-1
            state.update(track=track,level=level+1,phase='upgraded' if track==4 else 'buy',next=now+.3)
        elif phase=='upgraded':
            check(all(c.upgrade_level==4 and not c.coin_button.get_is_enabled() for c in state['cards']),'All four tracks reach level four and disable purchase')
            mining=state['mining'];flight=state['flight'];bomb=state['bomb']
            check(math.isclose(mining.radius,state['radius']*2) and math.isclose(mining.interval,state['interval']*.5),'Pickaxe upgrades apply')
            check(flight.creative_flight and math.isclose(flight.flight_settings.heat_up_seconds,state['heat']*2),'Jetpack upgrades apply')
            check(bomb.max_bombs==12 and math.isclose(bomb.settings.excavation_radius,state['blast']*2) and math.isclose(bomb.settings.fuse_seconds,1.5),'Bomb upgrades apply')
            check(mining.infinite_drill_energy and math.isclose(mining.max_drill_energy,150),'Drill upgrades apply')
            capture('03_maxed_upgrades');key('Tab',False);state['retained_coins']=state['wallet'].balance
            flight.set_editor_property('gravity_scale',1.)
            flight.set_movement_mode(unreal.MovementMode.MOVE_FALLING)
            state['jetpack_start_z']=state['pawn'].get_actor_location().z
            key('SpaceBar',True);state.update(phase='jetpack_release',next=now+.6)
        elif phase=='jetpack_release':
            key('SpaceBar',False)
            z=state['pawn'].get_actor_location().z
            result['jetpack_diagnostics']=dict(start_z=state['jetpack_start_z'],end_z=z,held=state['flight'].get_editor_property('bFlightInputHeld'),mode=str(state['flight'].movement_mode),velocity=list(state['flight'].velocity.to_tuple()),menu_held=state['pc'].get_editor_property('bUpgradeMenuHeld'))
            check(z>state['jetpack_start_z']+30,'Real Space input raises the jetpack-equipped player')
            state.update(phase='hover_start',next=now+.2)
        elif phase=='hover_start':
            state['hover_z']=state['pawn'].get_actor_location().z;state.update(phase='hover_check',next=now+.6)
        elif phase=='hover_check':
            check(abs(state['pawn'].get_actor_location().z-state['hover_z'])<5,'Creative jetpack upgrade holds altitude after releasing Space')
            key('LeftControl',True);state.update(phase='descend_check',next=now+.3)
        elif phase=='descend_check':
            key('LeftControl',False)
            check(state['pawn'].get_actor_location().z<state['hover_z']-10,'Real Ctrl input descends with the upgraded jetpack')
            state.update(phase='drill',next=now+.3)
        elif phase=='drill':
            key('V',True);key('V',False);state.update(phase='drill_check',next=now+11.1)
        elif phase=='drill_check':
            check(state['mining'].drill_mode and math.isclose(state['mining'].drill_energy,150),'Real V activates infinite-energy drill beyond original timeout')
            key('V',True);key('V',False);key('G',True);key('G',False);state.update(phase='throw',next=now+.4)
        elif phase=='throw':
            check(state['bomb'].equipped,'Real G equips bomb');state['explosions']=state['bomb'].explosion_count;state['bomb_count']=state['bomb'].bomb_count
            key('LeftMouseButton',True);key('LeftMouseButton',False);state.update(phase='bomb',next=now+2)
        elif phase=='bomb':
            check(state['bomb'].explosion_count>state['explosions'],'Real mouse input throws and detonates upgraded bomb')
            state.update(phase='travel',area=1,next=now+.3)
        elif phase=='travel':
            area=state['area'];state['pawn'].set_actor_location(state['hall'].areas[area].entry.translation,False,True)
            state.update(phase='travel_check',next=now+1)
        elif phase=='travel_check':
            area=state['area'];hall=state['hall']
            check(hall.active_area==area,'Physical area transition to '+str(area+1))
            check(state['wallet'].balance==state['retained_coins'] and all(state['progression'].get_level(i)==4 for i in range(4)),'Wallet and upgrades survive area transition')
            tag=str(hall.areas[area].tag);active=[i for i in state['field'].items if i.actor_has_tag(tag)]
            check(bool(active) and all(i.get_editor_property('CrystalClues').get_instance_count()>0 for i in active),'Crystal field activates in area '+str(area+1))
            if area==3:
                capture('04_last_region');finish(True)
            else:state.update(area=area+1,phase='travel',next=now+.2)
    except Exception:finish(False,traceback.format_exc())
    finally:state['busy']=False
callback=unreal.register_slate_post_tick_callback(tick)
