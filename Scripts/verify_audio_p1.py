import json
import pathlib
import numpy as np
import prepare_audio_pilot as audio


def main():
    entries = json.loads((audio.OUTPUT / 'manifest.json').read_text(encoding='utf-8'))
    report = []
    for entry in entries:
        samples, rate = audio.sf.read(audio.OUTPUT / 'Wav' / (entry['asset'] + '.wav'), always_2d=True)
        peak = float(np.max(np.abs(samples)))
        assert rate == 48000 and peak < .99 and np.isfinite(samples).all(), entry['asset']
        row = {'asset': entry['asset'], 'peak': peak, 'rms': float(np.sqrt(np.mean(samples ** 2)))}
        if entry['loop']:
            row['seam_delta'] = float(np.max(np.abs(samples[0] - samples[-1])))
            row['max_adjacent_delta'] = float(np.max(np.abs(np.diff(samples, axis=0))))
            assert row['seam_delta'] < max(.002, row['max_adjacent_delta'] * 1.1), entry['asset']
        report.append(row)
    music, rate = audio.sf.read(audio.OUTPUT / 'Wav/M_DreamyFlashback_Loop.wav', always_2d=True)
    seam = np.concatenate([music[-rate * 8:], music[:rate * 8]])
    audio.sf.write(audio.ROOT / 'Saved/AudioMusicSeam.wav', seam, rate, subtype='PCM_16')
    runtime_file = audio.ROOT / 'Saved/AudioP1Runtime.wav'
    result = {'assets': report, 'all_assets_valid': True, 'music_seam_preview_seconds': 16}
    if runtime_file.exists():
        runtime, rate = audio.sf.read(runtime_file, always_2d=True)
        result['runtime'] = {'seconds': len(runtime) / rate, 'peak': float(np.max(np.abs(runtime))),
                             'clipped_samples': int(np.count_nonzero(np.abs(runtime) >= .999)),
                             'rms': float(np.sqrt(np.mean(runtime ** 2)))}
        result['runtime']['one_second_rms'] = [round(float(np.sqrt(np.mean(runtime[start:start + rate] ** 2))), 6)
                                                for start in range(0, len(runtime), rate)]
        result['runtime']['master_mute_rms'] = float(np.sqrt(np.mean(runtime[17 * rate:round(17.8 * rate)] ** 2)))
        result['runtime']['restored_rms'] = float(np.sqrt(np.mean(runtime[19 * rate:20 * rate] ** 2)))
        assert result['runtime']['seconds'] >= 23 and result['runtime']['rms'] > .0001
        assert result['runtime']['clipped_samples'] == 0
        assert result['runtime']['master_mute_rms'] < .00001 and result['runtime']['restored_rms'] > .0001
    (audio.ROOT / 'Saved/AudioP1Validation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({'assets_valid': len(report), 'runtime': result.get('runtime'),
                      'music_seam_delta': next(row['seam_delta'] for row in report if row['asset'].startswith('M_'))}))


if __name__ == '__main__':
    main()
