"""PIE-only GUI verification: Tab focus handoff, camera, locked upgrades and mining."""
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
    check(helper.simulate_preview_button(button, True), button.get_name() + ' actual mouse hit target accepts press')
    check(helper.simulate_preview_button(button, False), button.get_name() + ' actual Slate click delegate runs')

def green_count(card):
    return sum(x.get_visibility() != unreal.SlateVisibility.HIDDEN for x in [card.green_segment1, card.green_segment2, card.green_segment3])

def rotation_change(a, b):
    return abs(a.yaw-b.yaw) + abs(a.pitch-b.pitch)

def key(name, down):
    helper.send_preview_key(state['pc'], name, down)

def release_tab_from_ui():
    check(helper.send_preview_focused_key(state['pc'], 'Tab', False), 'Tab release is handled through the focused Slate UI after clicking')

def tick(delta):
    try:
        now = time.monotonic()
        if now-state['started'] > 180:
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
            cards = [gui.pickaxe_upgrade, gui.vortex_upgrade, gui.blast_upgrade]
            check(gui.is_in_viewport(), 'Editable HUD automatically mounted')
            check(not gui.get_editor_property('bUpgradePanelOpen') and gui.upgrade_panel.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Upgrade panel starts hidden')
            check(not pc.show_mouse_cursor and not pc.get_editor_property('bUpgradeMenuHeld') and not pc.is_look_input_ignored(), 'Normal gameplay hides cursor and allows free look')
            check(not str(gui.coin_text.get_text()) and all(not str(x.price_text.get_text()) for x in cards), 'All currency and price areas remain empty')
            check([x.upgrade_level for x in cards] == [0,0,0], 'Cards start with zero upgraded segments')
            check(not cards[2].get_editor_property('bUnlocked') and not cards[2].coin_button.get_is_enabled(), 'Locked third item has a disabled coin button')
            check(not helper.simulate_preview_button(gui.plus_button, True), 'Plus artwork does not provide a competing popup toggle')
            mining = pawn.get_component_by_class(unreal.MiningComponent)
            effects = pawn.get_component_by_class(unreal.MiningToolEffectsComponent)
            check(mining and effects and effects.pickaxe_mesh, 'Existing mining and revised pickaxe remain present')
            pawn.get_component_by_class(unreal.CharacterMovementComponent).set_movement_mode(unreal.MovementMode.MOVE_NONE)
            rock = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DiggableRock)[0]
            origin, extent = rock.get_actor_bounds(False)
            pawn.set_actor_location(unreal.Vector(origin.x-extent.x-130, origin.y, origin.z), False, True)
            pc.set_control_rotation(unreal.Rotator())
            state.update(world=world, pc=pc, pawn=pawn, gui=gui, cards=cards, mining=mining, effects=effects, rock=rock, cuts=rock.successful_cuts, rotation=pc.get_control_rotation())
            shot('hud_default')
            helper.send_preview_mouse_axis(pc, 8, 4)
            state.update(phase='free_look', next=now+.2)
        elif phase == 'free_look':
            check(rotation_change(state['pc'].get_control_rotation(), state['rotation']) > .05, 'Mouse movement rotates camera without holding RMB')
            state['pc'].set_control_rotation(unreal.Rotator())
            key('G', True)
            state.update(phase='G', next=now+.2)
        elif phase == 'G':
            key('G', False)
            gui = state['gui']
            check(gui.ability_g.get_editor_property('bUsed') and not gui.ability_v.get_editor_property('bUsed'), 'G still changes only its supplied used icon')
            key('V', True)
            state.update(phase='V', next=now+.2)
        elif phase == 'V':
            key('V', False)
            check(state['mining'].get_editor_property('bDrillMode') and not state['gui'].ability_v.get_editor_property('bUsed'), 'V activates the timed drill; red icon is reserved for cooldown')
            state['mining'].reset_field()
            origin, extent = state['rock'].get_actor_bounds(False)
            state['pawn'].set_actor_location(unreal.Vector(origin.x-extent.x-130, origin.y, origin.z), False, True)
            state['pc'].set_control_rotation(unreal.Rotator())
            state['mining'].interval = 1.0
            key('LeftMouseButton', True)
            state.update(phase='held_mining', next=now+.2)
        elif phase == 'held_mining':
            check(state['rock'].successful_cuts > state['cuts'], 'Game mouse input starts actual mining before Tab')
            state.update(cuts=state['rock'].successful_cuts, rotation=state['pc'].get_control_rotation())
            key('Tab', True)
            check(state['gui'].get_editor_property('bUpgradePanelOpen') and state['pc'].show_mouse_cursor and state['pc'].is_look_input_ignored(), 'Tab immediately opens popup, exposes cursor and locks look')
            helper.send_preview_mouse_axis(state['pc'], 30, 15)
            state.update(phase='open_spring', spring_started=now, next=now)
        elif phase == 'open_spring':
            scale = state['gui'].upgrade_panel.get_editor_property('render_transform').scale.x
            state['max_scale'] = max(state['max_scale'], scale)
            if now-state['spring_started'] < 1.5:
                return
            check(state['max_scale'] > 1.015 and abs(scale-1) < .015, 'Tab popup retains physical overshoot and settles')
            report['panel_max_scale'] = state['max_scale']
            check(rotation_change(state['pc'].get_control_rotation(), state['rotation']) < .01, 'Pointer motion while upgrading does not rotate the camera')
            check(state['rock'].successful_cuts == state['cuts'], 'Opening Tab cancels an already-held mining click')
            for card in state['cards'][:2]:
                click(card.coin_button)
                check(card.upgrade_level == 1 and green_count(card) == 1, 'Unlocked item first click greens one segment')
            locked = state['cards'][2]
            check(not helper.simulate_preview_button(locked.coin_button, True), 'Locked item rejects actual button interaction')
            locked.request_upgrade()
            check(locked.upgrade_level == 0 and green_count(locked) == 0, 'Locked item also rejects direct upgrade requests')
            key('LeftMouseButton', True)
            key('LeftMouseButton', False)
            key('RightMouseButton', True)
            helper.send_preview_mouse_axis(state['pc'], 30, 15)
            key('RightMouseButton', False)
            state.update(phase='one_level', next=now+.5)
        elif phase == 'one_level':
            check(state['rock'].successful_cuts == state['cuts'], 'UI and unhandled blank-area mouse clicks do not mine')
            check(state['pc'].show_mouse_cursor and rotation_change(state['pc'].get_control_rotation(), state['rotation']) < .01, 'RMB cannot recapture the popup pointer or unlock the view')
            shot('hud_one_ring_and_used')
            state.update(phase='advance_levels', next=now+.2)
        elif phase == 'advance_levels':
            for card in state['cards'][:2]:
                for level in [2,3,3]:
                    click(card.coin_button)
                    check(card.upgrade_level == level and green_count(card) == level, 'Unlocked upgrades independently advance and stop at three')
            state['gui'].set_item_unlocked(2, True)
            check(state['cards'][2].coin_button.get_is_enabled(), 'Program unlock immediately enables the third coin button')
            state.update(phase='unlocked', next=now+.2)
        elif phase == 'unlocked':
            card = state['cards'][2]
            for level in [1,2,3]:
                click(card.coin_button)
                check(card.upgrade_level == level and green_count(card) == level, 'Previously locked item upgrades only after program unlock')
            state['gui'].set_item_unlocked(2, False)
            card.request_upgrade()
            check(not card.coin_button.get_is_enabled() and green_count(card) == 0, 'Relocking disables button and hides green overlays')
            state['gui'].set_item_level(2, 0, False)
            card = state['cards'][0]
            card.set_editor_property('bPreviewUpgrades', False)
            card.set_upgrade_level(0, False)
            click(card.coin_button)
            check(card.upgrade_level == 0, 'Gameplay can still own upgrade decisions independently')
            card.set_editor_property('bPreviewUpgrades', True)
            check(helper.simulate_preview_button(card.coin_button, True), 'Hold an upgrade button before releasing Tab')
            state.update(phase='release_over_button', next=now+.2)
        elif phase == 'release_over_button':
            check(state['cards'][0].price_content.get_editor_property('render_transform').scale.x < .95, 'Upgrade press retains Q-bounce compression')
            release_tab_from_ui()
            pc = state['pc']
            check(not pc.get_editor_property('bUpgradeMenuHeld') and not pc.show_mouse_cursor and not pc.is_look_input_ignored(), 'UI Tab release restores hidden cursor and camera control')
            check(not state['gui'].get_editor_property('bUpgradePanelOpen'), 'Tab release starts automatic popup return')
            check(not helper.simulate_preview_button(state['cards'][0].coin_button, False), 'Pending coin release cannot click after Tab is released')
            state['cards'][0].request_upgrade()
            check(state['cards'][0].upgrade_level == 0, 'Closing panel prevents deferred upgrade requests')
            state.update(phase='closed', next=now+1.2)
        elif phase == 'closed':
            check(state['gui'].upgrade_panel.get_visibility() == unreal.SlateVisibility.COLLAPSED, 'Popup collapses after its return animation')
            check(rotation_change(state['pc'].get_control_rotation(), state['rotation']) < .01, 'Cursor handoff creates no camera jump')
            check(state['rock'].successful_cuts == state['cuts'], 'Leaving popup does not resume the old held mining click')
            helper.send_preview_mouse_axis(state['pc'], 8, 4)
            state.update(phase='look_resumed', next=now+.2)
        elif phase == 'look_resumed':
            check(rotation_change(state['pc'].get_control_rotation(), state['rotation']) > .05, 'Mouse look resumes directly without another capture click')
            state['pc'].set_control_rotation(unreal.Rotator())
            key('LeftMouseButton', True)
            state.update(phase='mining_resumed', next=now+.3)
        elif phase == 'mining_resumed':
            key('LeftMouseButton', False)
            check(state['rock'].successful_cuts > state['cuts'], 'Fresh mouse click still mines after popup closes')
            check(state['effects'].swing_count > 0 and state['effects'].debris_burst_count > 0, 'Revised pickaxe animation and debris remain active')
            for _ in range(3):
                key('Tab', True)
                key('Tab', False)
            check(not state['pc'].is_look_input_ignored() and not state['pc'].show_mouse_cursor, 'Quick repeated Tab taps do not leave look locked or cursor exposed')
            gui = state['gui']
            gui.set_ability_used('G', False)
            gui.set_ability_used('V', False)
            for i in range(3):
                gui.set_item_level(i, 0, False)
            check(not str(gui.coin_text.get_text()) and all(not str(x.price_text.get_text()) for x in state['cards']), 'No currency numbers have been introduced')
            key('Tab', True)
            state.update(phase='final_shot', next=now+1.5)
        elif phase == 'final_shot':
            shot('hud_editable_panel')
            state.update(phase='finish', next=now+.3)
        elif phase == 'finish':
            release_tab_from_ui()
            finish(True, 'Tab hold/release, Slate UI focus, free-look handoff, locked upgrade gating, spring feedback and original mining passed')
    except Exception:
        finish(False, traceback.format_exc())

callback = unreal.register_slate_post_tick_callback(tick)
