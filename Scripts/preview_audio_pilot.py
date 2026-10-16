import pathlib
import sys

import numpy as np

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'Saved' / 'AudioPilotTools'))
import soundfile as sf

source = root / 'SourceAssets' / 'AudioPilot' / 'Wav'
rate = 48000
mix = np.zeros((24 * rate, 2), dtype=np.float32)
music, unused_rate = sf.read(source / 'M_DreamyFlashback_Loop.wav', dtype='float32', always_2d=True)
mix += music[:len(mix)] * .16


def add(name, seconds, gain):
    audio, sample_rate = sf.read(source / (name + '.wav'), dtype='float32', always_2d=True)
    if sample_rate != rate:
        raise RuntimeError('Unexpected sample rate')
    offset = round(seconds * rate)
    length = min(len(audio), len(mix) - offset)
    mix[offset:offset + length] += audio[:length] * gain * .7


for index in range(5):
    add(f'S_Foot_concrete_{index:02}', 2 + index * .48, .35)
for start in (6, 10, 14):
    for stage in range(3):
        add(f'S_Swing_{stage:02}', start + stage * .6, .32)
        add(('S_DigLight_' if stage == 0 else 'S_DigDeep_') + f'{stage:02}', start + stage * .6 + .19, .85 if stage == 0 else 1)
    add('S_DigBreak_00', start + 1.42, .9)
    add('S_Coin', start + 1.7, .45)
add('S_UI_Open', 18, .45)
add('S_UI_Confirm', 19, .6)
add('S_UI_Close', 20, .45)
mix[:rate] *= np.linspace(0, 1, rate)[:, None]
mix[-rate:] *= np.linspace(1, 0, rate)[:, None]
peak = float(np.max(np.abs(mix)))
if peak >= 1:
    raise RuntimeError(f'Mix clipping: {peak}')
output = root / 'Saved' / 'AudioPilotPreview.wav'
sf.write(output, mix, rate, subtype='PCM_16')
print(f'Preview: {output}; peak={peak:.3f}; duration=24s; offline montage, not gameplay capture')
