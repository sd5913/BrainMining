import argparse
import hashlib
import json
import pathlib
import re
import time

from verify_mobile_upscaling import DeviceSession, metrics


def wait_state(device, predicate, seconds=30):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        state = device.command('state')
        if predicate(state):
            return state
        time.sleep(.5)
    raise TimeoutError('Expected gameplay state was not reached')


def assert_water(state, name):
    surface = next(surface for surface in state['water_surfaces'] if surface['name'] == name)
    assert surface['visible'], surface
    assert surface['base_material'].endswith('/M_MobileWater.M_MobileWater'), surface


def compare_water(device, scene, surface, report):
    for mode, label in ((1, 'off'), (0, 'auto')):
        before = device.command('mobile_graphics', mode=mode)
        time.sleep(1)
        assert_water(device.command('state'), surface)
        device.command('sample_start', label=scene + '-' + label)
        time.sleep(6)
        sample = device.command('sample_end')
        assert sample['graphics']['mode'] == mode
        if mode == 1:
            assert sample['graphics']['r.ArmASR.Enable'] == 0
        else:
            assert sample['graphics']['asr_frames'] > before['graphics']['asr_frames']
        device.capture(scene + '-' + label)
        report[scene + '-' + label] = {'state': sample, 'frame_times': metrics(sample['frame_ms'])}


def click_tag(device, tag):
    for phase in ('move', 'down', 'up'):
        device.command('slate_pointer', tag=tag, phase=phase)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--serial', required=True)
    parser.add_argument('--apk', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=False)
    device = DeviceSession(options.serial, options.output)
    installed = device.adb('shell', 'pm', 'path', device.package).stdout.decode().strip()
    assert installed.startswith('package:') and '\n' not in installed
    with options.apk.open('rb') as stream:
        expected_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    actual_hash = device.adb('shell', 'sha256sum', installed.removeprefix('package:')).stdout.decode().split()[0]
    assert expected_hash == actual_hash, 'The installed APK differs from the requested build'
    report = {'apk_sha256': expected_hash, 'automated_checks_passed': False, 'visual_review_required': True}
    try:
        device.begin()
        device.command('start')
        wait_state(device, lambda state: not state['opening'])
        device.command('area_fixture', area=4)
        device.command('pose', eye=[36238, 15176, 18350], target=[36238, 16900, 18136])
        compare_water(device, 'heaven', 'heaven_basin', report)
        before = device.command('state')
        dimensions = re.findall(r'(\d+)x(\d+)', device.adb('shell', 'wm', 'size').stdout.decode())
        assert dimensions
        width, height = sorted(map(int, dimensions[-1]), reverse=True)
        start_x, start_y = round(width * .51), round(height * .30)
        device.adb('shell', 'input', 'swipe', str(start_x), str(start_y), str(start_x + round(height * .1)), str(start_y), '400')
        time.sleep(.4)
        after = device.command('state')
        yaw = (after['camera_rotation'][1] - before['camera_rotation'][1] + 180) % 360 - 180
        expected_yaw = before['mobile_look_sensitivity'] * 1440 * (.1 - 2 / 720)
        assert abs(yaw - expected_yaw) < 3, (yaw, expected_yaw)
        report['native_center_swipe_yaw'] = yaw
        device.command('pose', eye=[36238, 15176, 18350], target=[36238, 16900, 18136])
        device.command('water_fixture', water='heaven', diagnostic=True)
        device.capture('heaven-diagnostic')
        device.command('area_fixture', area=3)
        stage = device.command('actors', filter='CardRoomStage')['actors'][0]['position']
        device.command('pose', eye=[stage[0]-270, stage[1], stage[2]+188], target=[stage[0], stage[1], stage[2]+87])
        device.command('card', action='interact')
        for round_number in range(1, 5):
            wait_state(device, lambda state: state['card_phase'] == 4 and state['card_round'] == round_number)
            device.command('card', action='all')
            device.command('card', action='confirm')
        wait_state(device, lambda state: state['card_phase'] in (6, 7))
        device.command('card', action='interact')
        device.command('pose', eye=[stage[0]-1000, stage[1], stage[2]+1300], target=[stage[0]+1200, stage[1], stage[2]+180])
        wait_state(device, lambda state: state.get('flood_phase') == 2)
        device.command('water_fixture', water='card', height=stage[2]+180)
        compare_water(device, 'card', 'card_flood', report)
        device.command('water_fixture', water='card', height=stage[2]+1350)
        device.command('pose', eye=[stage[0]-1000, stage[1], stage[2]+1300], target=[stage[0]+1200, stage[1], stage[2]+1600])
        compare_water(device, 'card-underwater', 'card_flood', report)
        device.command('water_fixture', water='card', diagnostic=True)
        device.capture('card-underwater-diagnostic')
        device.command('water_fixture', water='card', height=stage[2]+180)
        device.command('pose', eye=[stage[0]-1000, stage[1], stage[2]+1300], target=[stage[0]+1200, stage[1], stage[2]+180])
        device.capture('card-diagnostic')
        device.command('gm_map', index=4)
        wait_state(device, lambda state: state['map'] == 'Lvl_EndingEmergence_Demo')
        device.command('film_fixture')
        state = wait_state(device, lambda state: state.get('ending_shot') == 9, seconds=180)
        assert state['ending_comfort']
        assert not state['camera_constrain_aspect']
        device.command('pause')
        click_tag(device, 'sliders')
        device.capture('comfort-settings')
        click_tag(device, 'MobileComfortMode')
        assert not device.command('state')['ending_comfort']
        click_tag(device, 'MobileComfortMode')
        assert device.command('state')['ending_comfort']
        device.command('pause')
        report['ending'] = []
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            state = device.command('state')
            if state['ending_shot'] != 9:
                break
            assert state['ending_comfort'] and not state['camera_constrain_aspect']
            assert abs(state['camera_rotation'][2]) <= 8, state['camera_rotation']
            if len(report['ending']) % 5 == 0:
                device.capture('ending-drill-' + str(len(report['ending'])))
            report['ending'].append({key: state[key] for key in ('camera_rotation', 'camera_fov', 'ending_shot')})
            time.sleep(1)
        assert len(report['ending']) >= 5 and state['ending_shot'] >= 10
        report['automated_checks_passed'] = True
    finally:
        try:
            device.close()
        finally:
            options.output.joinpath('report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'automated_checks_passed': True, 'visual_review_required': True, 'output': str(options.output)}))


if __name__ == '__main__':
    main()
