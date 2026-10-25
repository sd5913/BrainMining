import argparse
import hashlib
import json
import pathlib
import shlex
import statistics
import subprocess
import time
import uuid


class DeviceSession:
    package = 'com.YourCompany.BrainMining'
    override = 'files/UnrealGame/BrainMining/UECommandLine.txt'

    def __init__(self, serial, output):
        self.serial = serial
        self.output = output
        self.profile = f'/data/user/0/{self.package}/files/BMUpscalingQA-{uuid.uuid4().hex}'
        self.directory = self.profile + '/Saved/PackagedQA'
        self.commandline = f'../../../BrainMining/BrainMining.uproject -MiningQA -UserDir={self.profile}/\n'
        self.owned_override = False

    def adb(self, *arguments, data=None, check=True):
        return subprocess.run(['adb', '-s', self.serial, *arguments], input=data, capture_output=True, timeout=45, check=check)

    def shell(self, command, data=None, check=True):
        return self.adb('shell', '-T', 'run-as', self.package, 'sh', '-c', shlex.quote(command), data=data, check=check)

    def write(self, path, data):
        self.shell('cat > ' + shlex.quote(path + '.tmp') + ' && mv ' + shlex.quote(path + '.tmp') + ' ' + shlex.quote(path), data=data)

    def launch(self):
        self.adb('shell', 'am', 'force-stop', self.package)
        self.adb('shell', 'am', 'start', '-W', '-n', self.package + '/com.epicgames.unreal.SplashActivity')

    def begin(self):
        for override in (self.override, self.override.replace('UECommandLine', 'uecommandline')):
            result = self.shell('test -e ' + shlex.quote(override), check=False)
            if result.returncode != 1:
                raise RuntimeError('Existing command-line override or unavailable app access; no override changed')
        self.shell('mkdir -p ' + shlex.quote(self.directory))
        self.write(self.override, self.commandline.encode())
        self.owned_override = True
        self.output.joinpath('session.json').write_text(json.dumps({'profile': self.profile, 'override': self.override, 'content': self.commandline}, indent=2), encoding='utf-8')
        self.launch()

    def close(self):
        if not self.owned_override:
            return
        content = self.shell('cat ' + shlex.quote(self.override)).stdout.decode().replace('\r\n', '\n')
        if content != self.commandline:
            raise RuntimeError('Command-line override changed during QA; preserved for inspection')
        self.adb('shell', 'am', 'force-stop', self.package)
        self.shell('rm -- ' + shlex.quote(self.override))
        if self.shell('test -e ' + shlex.quote(self.override), check=False).returncode != 1:
            raise RuntimeError('QA override removal could not be verified')
        self.launch()
        self.output.joinpath('restored.json').write_text(json.dumps({'normal_launch_restored': True, 'player_data_cleared': False}), encoding='utf-8')

    def command(self, operation, timeout=45, **fields):
        request = dict(id=uuid.uuid4().hex, op=operation, **fields)
        self.write(self.directory + '/command.json', json.dumps(request).encode())
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            response = self.shell('cat ' + shlex.quote(self.directory + '/response.json'), check=False)
            try:
                result = json.loads(response.stdout.decode('utf-8-sig'))
            except (ValueError, UnicodeDecodeError):
                result = {}
            if result.get('id') == request['id']:
                with self.output.joinpath('transcript.jsonl').open('a', encoding='utf-8') as stream:
                    stream.write(json.dumps({'request': request, 'response': result}, ensure_ascii=False) + '\n')
                if not result.get('ok'):
                    raise RuntimeError(result)
                return result
            time.sleep(.2)
        raise TimeoutError(request)

    def capture(self, name):
        self.output.joinpath(name + '.png').write_bytes(self.adb('exec-out', 'screencap', '-p').stdout)


def metrics(samples):
    if not samples:
        return None
    ordered = sorted(samples)
    return dict(count=len(ordered), mean_ms=statistics.mean(ordered), p95_ms=ordered[min(len(ordered)-1, int(len(ordered)*.95))], max_ms=max(ordered))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--serial', required=True)
    parser.add_argument('--apk', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--seconds', type=int, default=20)
    parser.add_argument('--repeats', type=int, default=2)
    args = parser.parse_args()
    if args.seconds < 10 or args.repeats < 2:
        parser.error('Use at least ten seconds and two alternating-order repetitions')
    args.output.mkdir(parents=True, exist_ok=False)
    device = DeviceSession(args.serial, args.output)
    with args.apk.open('rb') as stream:
        expected_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
    package_path = device.adb('shell', 'pm', 'path', device.package).stdout.decode().strip()
    if '\n' in package_path or not package_path.startswith('package:'):
        raise RuntimeError('Expected one installed base APK')
    actual_hash = device.adb('shell', 'sha256sum', package_path.removeprefix('package:')).stdout.decode().split()[0]
    if actual_hash.lower() != expected_hash:
        raise RuntimeError('Installed APK differs from the supplied artifact')
    report = dict(apk_sha256=actual_hash, scope='idle scene fixtures; visual/motion/mining acceptance still required', samples=[])
    try:
        device.begin()
        initial = device.command('state')
        if not initial['opening'] or initial.get('has_save'):
            raise RuntimeError('Expected a fresh isolated profile')
        if 'graphics' not in initial:
            raise RuntimeError('Installed build lacks mobile graphics telemetry')
        if not initial['graphics']['asr_supported']:
            raise RuntimeError('ASR unavailable on this device/RHI; run fallback acceptance separately')
        device.command('start')
        time.sleep(5)
        for area, scene in ((0, 'start-room'), (1, 'office'), (4, 'third-mine'), (5, 'museum')):
            device.command('area_fixture', area=area)
            if area == 1:
                office = device.command('actors', filter='PopupOfficeDevice')['actors'][0]
                target = [value + (124 if axis == 2 else 0) for axis, value in enumerate(office['position'])]
                eye = [value - office['forward'][axis] * 215 + (158 if axis == 2 else 0) for axis, value in enumerate(office['position'])]
                device.command('pose', eye=eye, target=target)
            for repeat in range(args.repeats):
                modes = (1, 2, 3, 4) if repeat % 2 == 0 else (4, 3, 2, 1)
                for mode in modes:
                    label = f'{scene}-r{repeat}-mode{mode}'
                    device.command('mobile_graphics', mode=mode)
                    time.sleep(5)
                    before = device.command('state')
                    battery = device.adb('shell', 'dumpsys', 'battery').stdout.decode()
                    device.command('sample_start', label=label)
                    time.sleep(args.seconds)
                    sample = device.command('sample_end')
                    graphics = sample['graphics']
                    if graphics['mode'] != mode or sample['area'] != area:
                        raise RuntimeError('Fixture or graphics mode changed')
                    rendered = graphics['asr_frames'] - before['graphics']['asr_frames']
                    if mode == 1 and (rendered != 0 or graphics['r.ArmASR.Enable'] != 0):
                        raise RuntimeError('ASR continued rendering after Off')
                    if mode != 1:
                        if rendered <= 0 or not graphics['asr_history_valid']:
                            raise RuntimeError('ASR did not render/reuse history')
                        ratio = graphics['r.ScreenPercentage'] / 100
                        for axis in ('width', 'height'):
                            if abs(graphics['asr_input_' + axis] - graphics['asr_output_' + axis] * ratio) > 4:
                                raise RuntimeError('Actual ASR dimensions do not match the preset')
                    device.capture(label)
                    raw = dict(before=before, sample=sample, battery=battery)
                    args.output.joinpath(label + '.json').write_text(json.dumps(raw, ensure_ascii=False, indent=2), encoding='utf-8')
                    row = dict(scene=scene, mode=mode, repeat=repeat, game_loop=metrics(sample['frame_ms']), gpu=metrics(sample['gpu_frame_ms']), gpu_disjoint=sample['gpu_disjoint_samples'])
                    report['samples'].append(row)
                    print(json.dumps(row), flush=True)
        device.command('mobile_graphics', mode=3)
        device.launch()
        if device.command('state')['graphics']['mode'] != 3:
            raise RuntimeError('Graphics setting did not persist across restart')
        report['functional_passed'] = True
    finally:
        args.output.joinpath('report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        device.close()


if __name__ == '__main__':
    main()
