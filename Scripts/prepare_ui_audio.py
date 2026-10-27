"""Create original, deterministic soft tactile UI sounds; no third-party samples."""
import array, json, math, random, wave
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'SourceAssets/UIAudio'
out.mkdir(parents=True, exist_ok=True)
rate = 48000
manifest = []
for name, duration, pitch, volume, seed in [
    ('S_UI_Hover', .055, 1250, .20, 41),
    ('S_UI_Click', .105, 850, .32, 42),
]:
    rng = random.Random(seed)
    values = []
    noise = 0.0
    for i in range(round(duration * rate)):
        t = i / rate
        noise += .28 * (rng.uniform(-1, 1) - noise)
        attack = min(1, t / .0015)
        tail = min(1, (duration-t) / .008)
        # Damped inharmonic resonances: a warm tap, without a sustained beep.
        body = sum(a * math.sin(2*math.pi*pitch*r*t) * math.exp(-t/d)
                   for a,r,d in [(1,1,.008),(.32,1.71,.005),(.16,.48,.015)])
        v = attack * tail * (body*.65 + noise*.5*math.exp(-t/.005))
        if name == 'S_UI_Click' and t > .012:
            u = t-.012
            v += .13*math.sin(2*math.pi*620*u)*min(1,u/.001)*math.exp(-u/.012)*tail
        values.append(v)
    peak = max(map(abs, values))
    pcm = array.array('h', (round(v/peak*.8*32767) for v in values))
    with wave.open(str(out/(name+'.wav')), 'wb') as f:
        f.setnchannels(1); f.setsampwidth(2); f.setframerate(rate); f.writeframes(pcm.tobytes())
    manifest.append(dict(asset=name, duration=duration, volume=volume, source='Original procedural synthesis', seed=seed))
(out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf8')
print(json.dumps(manifest))
