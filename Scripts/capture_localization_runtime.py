import json
import re
import time
import traceback
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
output = root / 'Saved/Validation/LocalizationAchievements/RuntimeScreens'
output.mkdir(parents=True, exist_ok=True)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
helper = unreal.MiningGUIEditorLibrary
cultures = ['zh-Hans', 'en', 'zh-Hant']
state = {'phase': 'load', 'started': time.monotonic(), 'next': 0, 'busy': False, 'images': []}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def press(name, focused=True):
    sender = helper.send_preview_focused_key if focused else helper.send_preview_key
    sender(state['controller'], name, True)
    sender(state['controller'], name, False)


def finish(error=None):
    (output / 'report.json').write_text(json.dumps({'passed': error is None, 'error': error, 'initial_language': state.get('initial_language'), 'areas': state.get('areas', {}), 'images': state['images']}, ensure_ascii=False, indent=2), encoding='utf-8')
    levels.editor_request_end_play()
    state.update(phase='cleanup', next=time.monotonic() + 2)


def begin_screen(screen):
    state.update(screen=screen, culture=0, phase='language', next=time.monotonic() + 0.5)


def tick(delta):
    if state['busy'] or time.monotonic() < state['next']:
        return
    state['busy'] = True
    try:
        if state['phase'] == 'cleanup':
            if editor.get_game_world():
                return
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.quit_editor()
            return
        assert time.monotonic() - state['started'] < 300, 'Runtime capture timed out: ' + state['phase']
        if state['phase'] == 'load':
            assert not editor.get_game_world(), 'Stop PIE before capture'
            assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(), 'mining.SaveEnabled 0')
            helper.start_floating_preview()
            state.update(phase='ready', next=time.monotonic() + 5)
            return
        world = editor.get_game_world()
        if not world:
            return
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        if not controller or not controller.get_controlled_pawn():
            return
        state['controller'] = controller
        if state['phase'] == 'ready':
            openings = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningOpening)
            if not openings:
                return
            menu = helper.read_preview_object(openings[0], 'Menu')
            if not menu:
                return
            instance = unreal.GameplayStatics.get_game_instance(world)
            state['localization'] = next(item for item in unreal.ObjectIterator(unreal.MiningLocalizationSubsystem) if item.get_outer() == instance)
            state['initial_language'] = {'preference': state['localization'].get_language_preference(), 'resolved': state['localization'].get_language()}
            state['menu'] = menu
            begin_screen('opening')
        elif state['phase'] == 'language':
            assert state['localization'].set_language_preference(cultures[state['culture']])
            state.update(phase='capture', next=time.monotonic() + 0.6)
        elif state['phase'] == 'capture':
            culture = cultures[state['culture']]
            assert state['localization'].get_language() == culture
            if state['screen'] == 'map':
                hall = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)[0]
                texts = [{'tag': str(area.tag), 'title': str(area.title), 'instructions': str(area.instructions)} for area in hall.areas]
                state.setdefault('areas', {})[culture] = texts
                if culture == 'en':
                    assert not any(re.search(r'[\u3400-\u9fff]', item['title'] + item['instructions']) for item in texts), 'English map contains untranslated area text'
            name = state['screen'] + '-' + culture + '.png'
            assert helper.capture_preview(controller, str(output / name)), name
            state['images'].append(name)
            state['culture'] += 1
            if state['culture'] < len(cultures):
                state['phase'] = 'language'
                return
            if state['screen'] == 'opening':
                button = helper.read_preview_object(state['menu'], 'SettingsButton')
                assert button, 'Main menu settings button is missing'
                assert helper.simulate_preview_button(button, True)
                assert helper.simulate_preview_button(button, False)
                assert unreal.GameplayStatics.is_game_paused(world), 'Opening settings did not open'
                begin_screen('opening-settings')
            elif state['screen'] == 'opening-settings':
                press('Escape')
                assert not unreal.GameplayStatics.is_game_paused(world), 'Opening settings did not close'
                assert controller.get_editor_property('show_mouse_cursor'), 'Opening cursor was not restored'
                state.update(phase='start_game', next=time.monotonic() + 0.5)
            elif state['screen'] == 'settings':
                press('Escape')
                state.update(phase='pause', next=time.monotonic() + 0.5)
            elif state['screen'] == 'pause':
                press('Escape')
                fields = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningCollectionField)
                field = next(item for item in fields if item.use_authored_placements)
                state['collection'] = field
                field.set_editor_property('area_managed', False)
                helper.write_preview_state(field, 'Inventory', '(' + ','.join(['1'] * 23) + ')')
                field.toggle_catalogue()
                begin_screen('collection')
            elif state['screen'] == 'collection':
                state['collection'].toggle_catalogue()
                hall = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)[0]
                assert hall.enter_area(0, True), 'Could not enter the first mine for map validation'
                state.update(phase='map', next=time.monotonic() + 2)
            elif state['screen'] == 'map':
                press('Escape')
                assert helper.prepare_collection_preview_area(controller, 1, False)
                hall = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningExhibitHall)[0]
                assert hall.enter_area(1, True), 'Could not enter the office for room-map validation'
                state.update(phase='map-room', next=time.monotonic() + 2)
            else:
                finish()
        elif state['phase'] == 'start_game':
            button = helper.read_preview_object(state['menu'], 'StartButton')
            assert helper.simulate_preview_button(button, True)
            assert helper.simulate_preview_button(button, False)
            state.update(phase='settings', next=time.monotonic() + 5)
        elif state['phase'] == 'settings':
            press('F8', False)
            assert unreal.GameplayStatics.is_game_paused(world), 'Settings did not open'
            begin_screen('settings')
        elif state['phase'] == 'pause':
            press('Escape')
            assert unreal.GameplayStatics.is_game_paused(world), 'Pause menu did not open'
            begin_screen('pause')
        elif state['phase'] in ('map', 'map-room'):
            press('M', False)
            assert unreal.GameplayStatics.is_game_paused(world), 'Campaign map did not open'
            begin_screen(state['phase'])
    except Exception:
        finish(traceback.format_exc())
    finally:
        state['busy'] = False


handle = unreal.register_slate_post_tick_callback(tick)
