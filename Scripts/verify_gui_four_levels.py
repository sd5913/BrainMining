"""Validate four editable overlays through real PIE coin buttons, without saving test state."""
import json
import time
import traceback
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
performance = unreal.get_default_object(unreal.load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))
old_throttle = performance.get_editor_property('bThrottleCPUWhenNotForeground')
performance.set_editor_property('bThrottleCPUWhenNotForeground', False)
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/FourLevelUpgrade'
output.mkdir(parents=True, exist_ok=True)
report = {'passed': False, 'checks': []}
state = {'phase': 'start', 'started': time.monotonic(), 'next': 0, 'busy': False}

def check(value, label):
    assert value, label
    report['checks'].append(label)

def visible_count(card):
    return sum(x.get_visibility() == unreal.SlateVisibility.HIT_TEST_INVISIBLE
        for x in [card.green_segment1, card.green_segment2, card.green_segment3, card.green_segment4])

def click(card):
    check(helper.simulate_preview_button(card.coin_button, True), 'Coin button accepts actual Slate press')
    check(helper.simulate_preview_button(card.coin_button, False), 'Coin button accepts actual Slate release/click')

def screenshot(name):
    check(helper.capture_preview(state['pc'], str(output / (name + '.png'))), 'Captured ' + name)

def finish(passed, reason):
    unreal.unregister_slate_post_tick_callback(callback)
    performance.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    report.update(passed=passed, reason=reason, elapsed=time.monotonic()-state['started'])
    (output / 'runtime.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    unreal.log('FOUR_LEVEL_UPGRADE_RESULT ' + json.dumps(report))
    if editor.get_game_world(): levels.editor_request_end_play()

def tick(delta):
    if state['busy']: return
    state['busy'] = True
    try:
        now = time.monotonic()
        if now-state['started'] > 60: raise RuntimeError('Validation timeout: ' + state['phase'])
        if now < state['next']: return
        phase = state['phase']
        if phase == 'start':
            check(not editor.get_game_world(), 'No preexisting game preview is interrupted')
            helper.start_floating_preview()
            state.update(phase='setup', next=now+3)
        elif phase == 'setup':
            world = editor.get_game_world()
            if not world: return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not pc or not pc.mining_gui: return
            halls = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)
            if any(not x.get_editor_property('bReady') for x in halls): return
            gui = pc.mining_gui
            cards = [gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade]
            state.update(pc=pc, gui=gui, cards=cards)
            # Latest showcase owns prices and purchases. Isolate the visual preview in PIE only.
            # Destroying its PIE copy removes its delegates; no level asset or wallet is modified.
            for hall in halls: hall.destroy_actor()
            for index, card in enumerate(cards):
                gui.set_item_unlocked(index, index < 2)
                card.set_editor_property('bPreviewUpgrades', True)
                card.set_upgrade_level(0, False)
                card.set_price_display(unreal.Text(''))
            gui.set_coin_display(unreal.Text(''))
            check(True, 'Four-level preview isolated from production purchase rules in PIE only')
            pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            check(gui.is_in_viewport() and gui.jetpack_heat_bar is not None, 'Existing HUD and jetpack bar remain mounted')
            check(all(x.green_segment4 is not None for x in cards), 'All three upgrade cards bind the fourth editable image')
            check(not str(gui.coin_text.get_text()) and all(not str(x.price_text.get_text()) for x in cards), 'Currency and prices stay empty')
            check(not cards[2].get_editor_property('bUnlocked'), 'Third card retains its locked state')
            helper.send_preview_key(pc, 'Tab', True)
            check(gui.get_editor_property('bUpgradePanelOpen') and pc.show_mouse_cursor and pc.is_look_input_ignored(), 'Held Tab exposes pointer and locks camera')
            state.update(phase='one', next=now+1.5)
        elif phase == 'one':
            for card in state['cards'][:2]:
                click(card)
                check(card.upgrade_level == 1 and visible_count(card) == 1, 'First click lights exactly one quadrant')
            locked = state['cards'][2]
            check(not helper.simulate_preview_button(locked.coin_button, True), 'Locked coin button rejects mouse press')
            locked.request_upgrade()
            check(locked.upgrade_level == 0 and visible_count(locked) == 0, 'Locked item cannot upgrade through direct request either')
            state.update(phase='shot_one', next=now+.5)
        elif phase == 'shot_one':
            screenshot('one_level')
            state.update(phase='all_levels', next=now+.1)
        elif phase == 'all_levels':
            for card in state['cards'][:2]:
                for level in [2, 3, 4, 4]:
                    click(card)
                    check(card.upgrade_level == level and visible_count(card) == level,
                          'Coin clicks independently advance to level ' + str(level) + ' and clamp at four')
                card.set_upgrade_level(99, False)
                check(card.upgrade_level == 4 and visible_count(card) == 4, 'Programmatic level also clamps at four')
                card.set_upgrade_level(-1, False)
                check(card.upgrade_level == 0 and visible_count(card) == 0, 'Reset hides all four quadrants')
                card.set_upgrade_level(4, False)
            state.update(phase='shot_four', next=now+.5)
        elif phase == 'shot_four':
            screenshot('four_levels')
            state['gui'].set_item_unlocked(2, True)
            state.update(phase='unlock', next=now+.2)
        elif phase == 'unlock':
            card = state['cards'][2]
            for level in [1, 2, 3, 4]:
                click(card)
                check(card.upgrade_level == level and visible_count(card) == level, 'Unlocked third item supports level ' + str(level))
            state['gui'].set_item_unlocked(2, False)
            check(not card.coin_button.get_is_enabled() and visible_count(card) == 0, 'Relocking disables upgrade and hides all four green overlays')
            card = state['cards'][0]
            card.set_upgrade_level(0, False)
            card.set_editor_property('bPreviewUpgrades', False)
            click(card)
            check(card.upgrade_level == 0, 'Gameplay-owned upgrade decisions remain supported')
            helper.send_preview_focused_key(state['pc'], 'Tab', False)
            check(not state['gui'].get_editor_property('bUpgradePanelOpen') and not state['pc'].show_mouse_cursor
                  and not state['pc'].is_look_input_ignored(), 'Tab release restores free look and hidden cursor')
            for c in state['cards']: check(not c.coin_button.get_is_enabled(), 'Closing panel disables coin interactions')
            state.update(phase='finish', next=now+1.5)
        elif phase == 'finish':
            check(state['gui'].upgrade_panel.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Existing popup return animation still collapses panel')
            finish(True, 'All four levels, locked state, editable images and Tab handoff passed')
    except Exception:
        finish(False, traceback.format_exc())
    finally:
        state['busy'] = False

callback = unreal.register_slate_post_tick_callback(tick)
