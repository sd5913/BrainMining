import json
import math
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/TreasureProgression'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': []}
state = {'phase': 'setup', 'started': time.monotonic(), 'next': 0, 'busy': False}
prices = [[50, 1000, 5000, 50000], [250, 7500, 25000, 125000],
          [1500, 10000, 40000, 160000], [20000, 100000, 500000, 2000000]]
values = [[2,3,4,5], [8,10,15,20], [40,60,80,100], [200,300,400,500],
          [750,1000,1500,2000], [4000,6000,8000,10000], [15000,25000,35000,50000],
          [75000,125000,175000,250000], [400000,600000,800000,1000000]]

def check(condition, message):
    assert condition, message
    report['checks'].append(message)

def key(name, pressed):
    helper.send_preview_key(state['pc'], name, pressed)

def screenshot(name):
    check(helper.capture_preview(state['pc'], str(output / (name + '.png'))), 'Captured ' + name)

def finish(passed):
    report.update(passed=passed, elapsed=time.monotonic()-state['started'])
    (output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('TREASURE_PROGRESSION_RESULT ' + json.dumps(report))
    if 'pc' in state:
        for name in ['Tab', 'SpaceBar', 'LeftControl', 'LeftMouseButton']:
            key(name, False)
    levels.editor_request_end_play()
    state.update(phase='exit', next=time.monotonic()+2)

def tick(delta):
    if state['busy']:
        return
    state['busy'] = True
    try:
        now = time.monotonic()
        if state['phase'] == 'exit':
            if now >= state['next'] and editor.get_game_world() is None:
                unreal.unregister_slate_post_tick_callback(callback)
                unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'QUIT_EDITOR')
            return
        if now-state['started'] > 180:
            raise RuntimeError('Timeout: '+state['phase'])
        if now < state['next']:
            return
        phase = state['phase']
        if phase == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if not halls or not halls[0].get_editor_property('bReady'):
                return
            hall = halls[0]
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            progression = pawn.get_component_by_class(unreal.MiningProgressionComponent)
            wallet = progression.get_wallet()
            gui = pc.mining_gui
            if not gui or not gui.drill_upgrade:
                return
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            flight = pawn.get_component_by_class(unreal.MiningCharacterMovementComponent)
            bomb = pawn.get_component_by_class(unreal.MiningBombToolComponent)
            state.update(world=world, hall=hall, pc=pc, pawn=pawn, progression=progression, wallet=wallet, gui=gui,
                         mining=mining, flight=flight, bomb=bomb, cards=[gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade, gui.drill_upgrade],
                         radius=mining.radius, interval=mining.interval, heat=flight.flight_settings.heat_up_seconds,
                         cooling=flight.flight_settings.passive_cool_seconds, blast=bomb.settings.excavation_radius)
            check(hall.enter_area(1), 'Enter the live main map mineral area')
            check([list(entry.values_by_size) for entry in hall.treasure.definitions] == values, 'Existing saved map migrates all nine minerals to exact four-tier prices')
            check(len(hall.treasure.items) == 18, 'All eighteen minerals still fit inside their rocks')
            for item in hall.treasure.items:
                definition = next(entry for entry in hall.treasure.definitions if entry.id == item.treasure_id)
                check(item.value == definition.values_by_size[item.size_tier], 'Generated mineral value matches its size tier')
                check(math.isclose(item.get_editor_property('Model').get_editor_property('relative_scale3d').x, definition.scale.x*definition.size_multipliers[item.size_tier], rel_tol=1e-5), 'Physical model scale matches its value tier')
            check(not hall.purchase_upgrade(0, 1) and wallet.balance == 0, 'Insufficient funds cannot purchase or debit')
            check(not hall.purchase_upgrade(1, 2), 'Skipping levels is rejected')
            wallet.add_coins(5000000)
            key('Tab', True)
            state.update(phase='panel', next=now+2)
        elif phase == 'panel':
            check(state['gui'].ability_g.cooldown_text is not None, 'Bomb inventory has a visible HUD label')
            check(all(card.green_segment4 for card in state['cards']), 'All four cards retain four editable level segments')
            check(all(card.coin_button.get_is_enabled() for card in state['cards']), 'All four affordable upgrades can be clicked')
            screenshot('01_four_tracks')
            state.update(phase='buy', track=0, level=0, next=now+.2)
        elif phase == 'buy':
            track, level = state['track'], state['level']
            if track == 3 and level == 3:
                screenshot('02a_million_price')
            card = state['cards'][track]
            before = state['wallet'].balance
            check(helper.simulate_preview_button(card.coin_button, True), 'Upgrade button accepts press')
            check(helper.simulate_preview_button(card.coin_button, False), 'Upgrade button accepts release')
            check(state['progression'].get_level(track) == level+1, 'Actual UI purchase advances track %d to level %d' % (track, level+1))
            check(state['wallet'].balance == before-prices[track][level], 'Actual purchase deducts the document price')
            check(not state['hall'].purchase_upgrade(track, level+1), 'Duplicate purchase rejected')
            check(state['wallet'].balance == before-prices[track][level], 'Rejected duplicate does not charge')
            mining, flight, bomb = state['mining'], state['flight'], state['bomb']
            if track == 0:
                check(math.isclose(mining.depth_multiplier, 2), 'Pickaxe depth upgrade is active')
                if level >= 1: check(math.isclose(mining.radius, state['radius']*2), 'Pickaxe radius doubles')
                if level >= 2: check(math.isclose(mining.interval, state['interval']*.5), 'Pickaxe interval halves')
                if level == 3: check(state['wallet'].mineral_double_chance == .5, 'Pickaxe maximum grants 50 percent double reward')
            elif track == 1:
                check(math.isclose(flight.cooling_delay, .5), 'Jetpack cooling starts after half-second delay')
                if level == 1: check(math.isclose(flight.flight_settings.passive_cool_seconds, state['cooling']/2), 'Jetpack natural cooling rate doubles')
                if level >= 2: check(math.isclose(flight.flight_settings.heat_up_seconds, state['heat']*2), 'Jetpack heat capacity doubles')
                if level == 3: check(flight.get_editor_property('bCreativeFlight'), 'Creative flight unlocked')
            elif track == 2:
                check(math.isclose(bomb.recharge_seconds, 8), 'Bomb automatic recharge reduced to eight seconds')
                if level == 1: check(bomb.max_bombs == 6, 'Bomb capacity doubles to six')
                if level >= 2: check(math.isclose(bomb.settings.excavation_radius, state['blast']*2), 'Bomb excavation range doubles')
                if level == 3: check(bomb.max_bombs == 12 and math.isclose(bomb.settings.fuse_seconds, 1.5), 'Final bomb level doubles capacity again and halves fuse')
            else:
                check(math.isclose(mining.drill_recovery_rate, 15), 'Drill recovery increases by 50 percent')
                if level >= 1: check(math.isclose(mining.max_drill_energy, 150), 'Drill capacity increases by 50 percent')
                if level >= 2: check(math.isclose(mining.drill_consumption_rate, 7.5), 'Drill consumption reduced by 25 percent')
                if level == 3: check(mining.get_editor_property('bInfiniteDrillEnergy'), 'Infinite drill energy unlocked')
            if level == 3:
                check(not state['hall'].purchase_upgrade(track, 5), 'Level five cannot charge coins')
                track += 1
                level = -1
            state.update(track=track, level=level+1, phase='maxed' if track == 4 else 'buy', next=now+.3)
        elif phase == 'maxed':
            check(all(card.upgrade_level == 4 and not card.coin_button.get_is_enabled() for card in state['cards']), 'Maxed cards display four levels and disable purchase')
            screenshot('02_maxed_tracks')
            state['progression'].apply_upgrades()
            check(math.isclose(state['mining'].radius, state['radius']*2), 'Reapplying upgrades does not multiply effects again')
            check(state['bomb'].max_bombs == 12, 'Reapplying does not increase capacity again')
            key('Tab', False)
            state['balance'] = state['wallet'].balance
            state['hall'].return_to_lobby()
            state['hall'].enter_area(1)
            check(state['wallet'].balance == state['balance'] and all(state['progression'].get_level(index) == 4 for index in range(4)), 'All four upgrades and exact coin balance survive region travel')
            key('SpaceBar', True)
            state.update(phase='flight_release', next=now+.6)
        elif phase == 'flight_release':
            key('SpaceBar', False)
            state.update(phase='hover_start', next=now+.2)
        elif phase == 'hover_start':
            state['hover_z'] = state['pawn'].get_actor_location().z
            state.update(phase='hover_check', next=now+.6)
        elif phase == 'hover_check':
            check(abs(state['pawn'].get_actor_location().z-state['hover_z']) < 5, 'Real creative flight holds altitude without Space')
            key('LeftControl', True)
            state.update(phase='descend', next=now+.25)
        elif phase == 'descend':
            key('LeftControl', False)
            check(state['pawn'].get_actor_location().z < state['hover_z']-10, 'Real left Ctrl descends during creative flight')
            key('V', True); key('V', False)
            state.update(phase='drill', game_start=unreal.GameplayStatics.get_time_seconds(state['world']), next=now+.2)
        elif phase == 'drill':
            if unreal.GameplayStatics.get_time_seconds(state['world'])-state['game_start'] < 11:
                return
            check(state['mining'].get_editor_property('bDrillMode'), 'Unlimited drill survives the former ten-second timeout')
            check(math.isclose(state['mining'].drill_energy, 150), 'Unlimited drill energy remains full')
            key('V', True); key('V', False)
            state.update(phase='drill_stowed', next=now+.2)
        elif phase == 'drill_stowed':
            check(not state['mining'].get_editor_property('bDrillMode'), 'Pressing V again stows the energy-based drill')
            key('G', True); key('G', False)
            state.update(phase='bomb_ready', next=now+.2)
        elif phase == 'bomb_ready':
            state['bombs'] = state['bomb'].bomb_count
            state['explosions'] = state['bomb'].explosion_count
            check(state['bomb'].get_editor_property('bEquipped'), 'Real G equips the upgraded bomb')
            state['throw_time'] = unreal.GameplayStatics.get_time_seconds(state['world'])
            key('LeftMouseButton', True); key('LeftMouseButton', False)
            state.update(phase='bomb_thrown', next=now+.15)
        elif phase == 'bomb_thrown':
            check(state['bomb'].bomb_count == state['bombs']-1, 'Real throw consumes exactly one bomb')
            state.update(phase='explosion', next=now+2)
        elif phase == 'explosion':
            if unreal.GameplayStatics.get_time_seconds(state['world'])-state['throw_time'] < 1.8:
                return
            check(state['bomb'].explosion_count == state['explosions']+1, 'Upgraded bomb detonates with its shorter fuse')
            screenshot('03_ability_hud')
            check(unreal.GameplayStatics.get_player_pawn(state['world'], 0) == state['pawn'], 'Entire test uses the same pawn')
            tracks = list(state['progression'].tracks)
            steps = list(tracks[0].steps)
            steps[3].value = 1
            tracks[0].steps = steps
            state['progression'].tracks = tracks
            state['progression'].apply_upgrades()
            item = state['hall'].treasure.items[0]
            position = item.get_actor_location()
            rock = min(state['hall'].treasure.rocks, key=lambda candidate: (candidate.get_actor_location()-position).length())
            check(rock.mine_sphere(position, 1000), 'Expose the bonus-test mineral through actual rock excavation')
            state['pawn'].set_actor_location(position+unreal.Vector(0, 250, 0), False, True)
            camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), position))
            state.update(item=item, balance=state['wallet'].balance, phase='collect', next=now+1)
        elif phase == 'collect':
            check(state['item'].exposure > .5, 'Bonus test mineral is sufficiently exposed')
            camera = state['pawn'].get_component_by_class(unreal.CameraComponent)
            state['pc'].set_control_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_world_location(), state['item'].get_actor_location()))
            state.update(phase='pickup', next=now+.4)
        elif phase == 'pickup':
            key('E', True); key('E', False)
            state.update(phase='reward', next=now+.3)
        elif phase == 'reward':
            item = state['item']
            check(item.get_editor_property('bCollected'), 'Actual E input collects upgraded mineral')
            check(item.collected_value == item.value*2, 'Pickup bonus doubles payout without changing the size-based base value')
            check(state['wallet'].balance == state['balance']+item.collected_value, 'Shared wallet receives the doubled payout exactly once')
            key('E', True); key('E', False)
            state.update(phase='duplicate_reward', next=now+.3)
        elif phase == 'duplicate_reward':
            check(state['wallet'].balance == state['balance']+state['item'].collected_value, 'Repeated pickup cannot grant a second reward')
            tracks = list(state['progression'].tracks)
            steps = list(tracks[0].steps)
            steps[3].value = .5
            tracks[0].steps = steps
            state['progression'].tracks = tracks
            state['progression'].apply_upgrades()
            check(state['wallet'].mineral_double_chance == .5, 'Document probability restored after deterministic bonus test')
            finish(True)
    except Exception:
        report['error'] = traceback.format_exc()
        finish(False)
    finally:
        state['busy'] = False

levels.load_level('/Game/Showcase/Maps/Lvl_MainExperiences')
helper.start_floating_preview()
callback = unreal.register_slate_post_tick_callback(tick)
