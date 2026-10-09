"""Run with -ExecutePythonScript in a GUI editor. Changes PIE only, never saves test state."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
output = Path(unreal.Paths.project_saved_dir()) / 'Validation' / 'MiningGUI'
output.mkdir(parents=True, exist_ok=True)
report = {'checks': [], 'passed': False}
state = {'phase': 'start', 'started': time.monotonic(), 'next': 0, 'max_scale': 0}
helper = unreal.MiningGUIEditorLibrary

def check(value, label):
    assert value, label
    report['checks'].append(label)

def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=passed, reason=reason, elapsed=time.monotonic()-state['started'])
    (output / 'runtime.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('MINING_GUI_RESULT ' + json.dumps(report))
    if editor.get_game_world():
        levels.editor_request_end_play()

def shot(name):
    unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI filename="' + str(output / (name+'.png')) + '" -nosuffix', state['pc'])

def click(button):
    check(helper.simulate_preview_button(button, True), button.get_name() + ' is the actual mouse hit target and press delegate runs')
    check(helper.simulate_preview_button(button, False), button.get_name() + ' Slate click delegate runs')

def green_count(card):
    return sum(segment.get_visibility() != unreal.SlateVisibility.HIDDEN for segment in
               [card.green_segment1, card.green_segment2, card.green_segment3])

def tick(delta):
    try:
        now = time.monotonic()
        if now - state['started'] > 180:
            raise RuntimeError('GUI validation timeout at ' + state['phase'])
        if now < state['next']:
            return
        phase = state['phase']
        if phase == 'start':
            check(levels.load_level('/Game/MiningPrototype/Maps/Lvl_MiningLab'), 'Original mining lab loads')
            helper.start_floating_preview()
            state.update(phase='setup', next=now+3)
        elif phase == 'setup':
            world = editor.get_game_world()
            if not world:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            if not pc or not pawn:
                return
            gui = pc.mining_gui
            check(gui and gui.is_in_viewport(), 'HUD automatically mounted by current local player controller')
            check(not gui.get_editor_property('bUpgradePanelOpen') and gui.upgrade_panel.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Three upgrade cards are initially hidden')
            check(not str(gui.coin_text.get_text()), 'Balance area remains empty for gameplay integration')
            cards = [gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade]
            check(all(not str(card.price_text.get_text()) for card in cards), 'All price areas are empty; no placeholder numbers are shown')
            check([x.upgrade_level for x in cards] == [0,0,0], 'All three editable upgrade cards start at zero')
            check(not cards[2].get_editor_property('bUnlocked') and cards[2].lock_icon.get_visibility() != unreal.SlateVisibility.HIDDEN, 'Third card has a separate visible lock layer')
            check(not gui.ability_g.get_editor_property('bUsed') and not gui.ability_v.get_editor_property('bUsed'), 'G and V initially use normal supplied icons')
            check(pc.show_mouse_cursor, 'Directly clickable UI has a visible mouse cursor')
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            effects = pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
            check(mining and effects and effects.pickaxe_mesh, 'Existing mining and pickaxe remain present')
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            rock = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock)[0]
            origin, extent = rock.get_actor_bounds(False)
            pawn.set_actor_location(unreal.Vector(origin.x-extent.x-130, origin.y, origin.z), False, True)
            pc.set_control_rotation(unreal.Rotator())
            state.update(world=world, pc=pc, gui=gui, cards=cards, mining=mining, effects=effects, rock=rock, cuts=rock.successful_cuts)
            shot('hud_default')
            state.update(phase='begin_G', next=now+.3)
        elif phase == 'begin_G':
            helper.send_preview_key(state['pc'], 'G', True)
            state.update(phase='G', next=now+.2)
        elif phase == 'G':
            gui = state['gui']
            helper.send_preview_key(state['pc'], 'G', False)
            check(gui.ability_g.get_editor_property('bUsed') and not gui.ability_v.get_editor_property('bUsed'), 'Real G input changes only the G icon')
            check(gui.ability_g.ability_icon.get_editor_property('brush').resource_object == gui.ability_g.used_texture, 'G uses the original red-slash PNG')
            helper.send_preview_key(state['pc'], 'V', True)
            state.update(phase='V', next=now+.2)
        elif phase == 'V':
            helper.send_preview_key(state['pc'], 'V', False)
            gui = state['gui']
            check(gui.ability_v.get_editor_property('bUsed'), 'Real V input changes the V icon')
            check(gui.ability_v.ability_icon.get_editor_property('brush').resource_object == gui.ability_v.used_texture, 'V uses the original red-slash PNG')
            check(helper.simulate_preview_button(gui.plus_button, True), 'Plus is hit-testable and press reaches its delegate')
            state.update(phase='plus_pressed', next=now+.2)
        elif phase == 'plus_pressed':
            gui = state['gui']
            check(gui.plus_content.get_editor_property('render_transform').scale.x < .95, 'Pressed plus visibly compresses')
            check(helper.simulate_preview_button(gui.plus_button, False), 'Plus release reaches actual Slate click delegate')
            check(gui.get_editor_property('bUpgradePanelOpen'), 'Clicking plus opens the three-card panel')
            state.update(phase='open_spring', spring_started=now, next=now)
        elif phase == 'open_spring':
            gui = state['gui']
            scale = gui.upgrade_panel.get_editor_property('render_transform').scale.x
            state['max_scale'] = max(state['max_scale'], scale)
            if now - state['spring_started'] < 1.5:
                return
            check(state['max_scale'] > 1.015, 'Panel springs past its target for physical bounce feedback')
            check(abs(scale-1) < .015, 'Panel spring settles at the designer size')
            report['panel_max_scale'] = state['max_scale']
            check(state['rock'].successful_cuts == state['cuts'], 'Clicking UI does not also mine the rock')
            for card in state['cards']:
                click(card.coin_button)
                check(card.upgrade_level == 1 and green_count(card) == 1, 'Card ' + str(card.item_index) + ': first click greens exactly one segment')
            state.update(phase='one_level', next=now+1.1)
        elif phase == 'one_level':
            shot('hud_one_ring_and_used')
            state.update(phase='advance_levels', next=now+.3)
        elif phase == 'advance_levels':
            for card in state['cards']:
                click(card.coin_button)
                check(card.upgrade_level == 2 and green_count(card) == 2, 'Card ' + str(card.item_index) + ': second click greens two segments')
                click(card.coin_button)
                check(card.upgrade_level == 3 and green_count(card) == 3, 'Card ' + str(card.item_index) + ': third click greens all three segments')
                click(card.coin_button)
                check(card.upgrade_level == 3, 'Card ' + str(card.item_index) + ': fourth click stays at three')
            gui = state['gui']
            check(not str(gui.coin_text.get_text()), 'Upgrade preview never invents or subtracts currency')
            gui.set_coin_display(' ')
            check(str(gui.coin_text.get_text()) == ' ', 'Balance text accepts program updates')
            gui.set_coin_display('')
            gui.set_ability_available('G', False)
            check(gui.ability_g.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Unavailable bottom-left ability is hidden')
            gui.set_ability_available('G', True)
            gui.set_item_unlocked(2, True)
            check(state['cards'][2].lock_icon.get_visibility() == unreal.SlateVisibility.HIDDEN, 'Gameplay can replace the separate lock layer with the item icon')
            gui.set_item_unlocked(2, False)
            state['cards'][0].set_editor_property('bPreviewUpgrades', False)
            gui.set_item_level(0, 0, False)
            click(state['cards'][0].coin_button)
            check(state['cards'][0].upgrade_level == 0, 'Gameplay can disable preview upgrading while keeping the button')
            state['cards'][0].set_editor_property('bPreviewUpgrades', True)
            gui.set_item_level(0, 3, False)
            click(gui.plus_button)
            state.update(phase='closed', next=now+1.2)
        elif phase == 'closed':
            gui = state['gui']
            check(not gui.get_editor_property('bUpgradePanelOpen') and gui.upgrade_panel.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Clicking plus again collapses the panel after its return animation')
            gui.set_ability_used('G', False)
            gui.set_ability_used('V', False)
            for i in range(3):
                gui.set_item_level(i, 0, False)
            state['mining'].reset_field()
            state['mining'].interval = 1.0
            check(state['mining'].try_mine(), 'Original mining still removes rock while the GUI is mounted')
            state.update(phase='mining', next=now+.3)
        elif phase == 'mining':
            check(state['effects'].swing_count > 0 and state['effects'].debris_burst_count > 0, 'Original pickaxe swing and debris still trigger with the GUI')
            click(state['gui'].plus_button)
            state.update(phase='final_shot', next=now+1.5)
        elif phase == 'final_shot':
            shot('hud_editable_panel')
            state.update(phase='finish', next=now+1)
        elif phase == 'finish':
            finish(True, 'UMG overlay, key input, button hit tests/click delegates, three-stage rings, spring feedback and mining passed')
    except Exception:
        finish(False, traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
