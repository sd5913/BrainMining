import hashlib
import html
import io
import json
import pathlib
import subprocess
import sys
import zipfile
from urllib.parse import quote

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Saved' / 'AudioPilotTools'))
import soundfile as sf

OUTPUT = ROOT / 'SourceAssets' / 'MainGameAudio'
RATE = 48000
SOURCES = {entry['id']: entry for entry in json.loads((OUTPUT / 'sources.json').read_text(encoding='utf-8'))}
RECORDS = []


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_source(source_id, member):
    source = SOURCES[source_id]
    archive_path = OUTPUT / source['file']
    if archive_path.suffix == '.zip':
        with zipfile.ZipFile(archive_path) as archive:
            raw = archive.read(member)
    elif archive_path.suffix == '.7z':
        raw = subprocess.check_output(['tar', '-xOf', str(archive_path), member])
    else:
        raw = archive_path.read_bytes()
    original = OUTPUT / 'Originals' / source_id / pathlib.PurePosixPath(member).name
    original.parent.mkdir(parents=True, exist_ok=True)
    original.write_bytes(raw)
    samples, rate = sf.read(io.BytesIO(raw), dtype='float32', always_2d=True)
    return samples, rate, original, digest(raw)


def candidate(name, label, priority, source_id, member, start=0, end=None, stereo=False, loop=False):
    samples, rate, original, source_hash = read_source(source_id, member)
    full_seconds = len(samples) / rate
    end = full_seconds if end is None else min(end, full_seconds)
    samples = samples[round(start * rate):round(end * rate)].copy()
    if len(samples) < 2 or not np.isfinite(samples).all():
        raise ValueError(name + ': invalid samples')
    if not stereo:
        samples = samples.mean(axis=1, keepdims=True)
    if rate != RATE:
        old_time = np.arange(len(samples))
        new_time = np.arange(round(len(samples) * RATE / rate)) * rate / RATE
        samples = np.stack([np.interp(new_time, old_time, samples[:, channel]) for channel in range(samples.shape[1])], axis=1)
    samples -= samples.mean(axis=0, keepdims=True)
    crossfade = 0
    if loop:
        crossfade = min(round(.25 * RATE), len(samples) // 8)
        blend = np.linspace(0, 1, crossfade)[:, None]
        overlap = samples[-crossfade:] * (1 - blend) + samples[:crossfade] * blend
        samples = np.concatenate([samples[crossfade:-crossfade], overlap])
    else:
        fade = min(round((.25 if stereo else .005) * RATE), len(samples) // 8)
        samples[:fade] *= np.linspace(0, 1, fade)[:, None]
        samples[-fade:] *= np.linspace(1, 0, fade)[:, None]
    peak = float(np.max(np.abs(samples)))
    if peak <= 1e-6:
        raise ValueError(name + ': silent candidate')
    target_peak = .4 if stereo else .63
    gain = min(target_peak / peak, 4)
    samples *= gain
    destination = OUTPUT / 'Candidates' / (name + '.wav')
    destination.parent.mkdir(parents=True, exist_ok=True)
    sf.write(destination, samples, RATE, subtype='PCM_16')
    decoded, decoded_rate = sf.read(destination, always_2d=True)
    if decoded_rate != RATE or not np.isfinite(decoded).all() or np.max(np.abs(decoded)) >= .999:
        raise ValueError(name + ': output validation failed')
    record = dict(id=name, label=label, priority=priority, source_id=source_id,
                  source_member=member, source_sha256=source_hash,
                  original=str(original.relative_to(OUTPUT)).replace('\\', '/'),
                  file=str(destination.relative_to(OUTPUT)).replace('\\', '/'),
                  source_seconds=full_seconds, source_excerpt_seconds=[start, end],
                  seconds=round(len(decoded) / RATE, 4), channels=decoded.shape[1], sample_rate=RATE,
                  peak=round(float(np.max(np.abs(decoded))), 6), rms=round(float(np.sqrt(np.mean(decoded ** 2))), 6),
                  clipped_samples=int(np.count_nonzero(np.abs(decoded) >= .999)),
                  loop_candidate=loop, crossfade_seconds=crossfade / RATE, gain=round(gain, 6),
                  boundary_jump=round(float(np.max(np.abs(decoded[0] - decoded[-1]))), 6),
                  processing='Selected excerpt; mono unless stereo specified; linear resample to 48 kHz; remove DC; crossfade loop or edge fades; peak trim; PCM16.',
                  sha256=digest(destination.read_bytes()), status='downloaded_candidate_not_imported',
                  listening_status='Pending human audition and MainGame playtest')
    RECORDS.append(record)


def build_catalog():
    panels = []
    runtime_path = OUTPUT / 'runtime_manifest.json'
    if runtime_path.exists():
        for record in json.loads(runtime_path.read_text(encoding='utf-8')):
            panels.append(f'<article data-kind="Runtime"><h3>接入版 · {html.escape(record["asset"])}</h3><p>{record["category"]} · {record["seconds"]} s</p>'
                          f'<audio controls preload="none" src="{quote(record["file"])}"></audio>'
                          '<small>已生成正式派生音源；编译与实图验收状态请查看 MainGameAudioImplementation.md。</small></article>')
    for record in RECORDS:
        source = SOURCES[record['source_id']]
        tags = f"{record['priority']} · {source['license']} · {record['seconds']} s"
        panels.append(f'<article data-kind="{record["priority"]}"><h3>{html.escape(record["label"])}</h3><p>{tags}</p>'
                      f'<code>{record["id"]}</code><audio controls preload="none" src="{record["file"]}"></audio>'
                      f'<p><a href="{quote(record["original"])}">完整原始片段</a> · '
                      f'<a href="{html.escape(source["page_url"])}" target="_blank" rel="noopener">{html.escape(source["title"])}</a></p>'
                      f'<small>{html.escape(source["author"])}</small></article>')
    pilot = json.loads((ROOT / 'SourceAssets/AudioPilot/manifest.json').read_text(encoding='utf-8'))
    for record in pilot:
        name = record['asset']
        panels.append(f'<article data-kind="Existing"><h3>当前版本 · {html.escape(name)}</h3><p>现有素材 · {record["seconds"]} s</p>'
                      f'<audio controls preload="none" src="../AudioPilot/Wav/{name}.wav"></audio>'
                      '<small>来源与加工记录见 AudioPilot/manifest.json；工具合成音仍是项目原有版本。</small></article>')
    document = '''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MainGame 音源候选试听</title><style>
body{margin:0;background:#111b24;color:#e2ebef;font:16px/1.6 system-ui,sans-serif}main{max-width:1200px;margin:auto;padding:32px 24px}
h1{font-size:28px;margin-bottom:8px}p{color:#bacbd5}nav{display:flex;gap:10px;flex-wrap:wrap;margin:24px 0}
button,input{background:#213445;color:#edf4f8;border:1px solid #526f80;padding:10px 14px;border-radius:8px;font:inherit}
button{cursor:pointer}button.active{background:#256e69}#grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(290px,1fr));gap:16px}
article{background:#1b2b37;border:1px solid #344c5c;border-radius:12px;padding:18px}article h3{font-size:18px;margin:0 0 8px}
article p{margin:8px 0}code,small{overflow-wrap:anywhere}code{font-size:12px;color:#91cfbf}audio{width:100%;margin:14px 0 4px}
a{color:#8de0d0}small{color:#b4c7d3}[hidden]{display:none!important}</style><main>
<h1>MainGame 音源候选试听</h1><p>卧室装备 → 喷气竖井 → 普通矿道 → 溶洞 → 石笋 → 神经岩。2026-10-16</p>
<p>候选音乐为 20 秒试听节选；接入版是另外整理的正式派生资源。导入、编译和实图验收是不同阶段，当前状态见接入记录。建议先调低系统音量。</p>
<p><a href="../../Docs/MainGameAudioImplementation.md">MainGame 接入与验证状态</a></p>
<p><a href="../../Docs/MainGameAudioResources.md">完整需求与接入缺口</a> · <a href="Licenses/CREDITS.txt">署名文本</a> · <a href="sources.json">来源清单</a></p>
<nav><button class="active" data-filter="New">新候选</button><button data-filter="Runtime">接入版</button><button data-filter="P0">P0</button><button data-filter="P1">P1</button><button data-filter="P2">备选</button><button data-filter="Existing">旧版本</button><button data-filter="All">全部</button><input id="search" aria-label="搜索音源" placeholder="搜索：钻机、喷气、音乐…"></nav>
<p id="count"></p><section id="grid">''' + ''.join(panels) + '''</section></main><script>
let filter='New';const cards=[...document.querySelectorAll('article')];const search=document.querySelector('#search');
function update(){let count=0;for(const card of cards){const kind=card.dataset.kind;const show=(filter==='All'||filter===kind||(filter==='New'&&['P0','P1','P2'].includes(kind)))&&card.textContent.toLowerCase().includes(search.value.toLowerCase());card.hidden=!show;if(show)count++;else card.querySelector('audio').pause();}document.querySelector('#count').textContent=count+' 个试听条目';}
document.querySelectorAll('button').forEach(button=>button.onclick=()=>{filter=button.dataset.filter;document.querySelectorAll('button').forEach(item=>item.classList.toggle('active',item===button));update();});search.oninput=update;
document.querySelectorAll('audio').forEach(audio=>{audio.volume=.35;audio.addEventListener('play',()=>document.querySelectorAll('audio').forEach(other=>{if(other!==audio)other.pause();}));});update();
</script></html>'''
    (OUTPUT / 'Audition.html').write_text(document, encoding='utf-8')


def main():
    for source in SOURCES.values():
        if digest((OUTPUT / source['file']).read_bytes()) != source['sha256']:
            raise ValueError(source['id'] + ': download checksum mismatch')
    candidate('C_PickaxePickup', '拿起镐子：短金属与工具碰撞', 'P0', 'workshop', 'workshop/workshop - tool clatter.wav')
    candidate('C_JetpackEquip', '背包装备：扣合候选', 'P0', 'workshop', 'workshop/workshop - clink thud.wav')
    candidate('C_DirtDig', '泥土层挖掘：土壤摩擦', 'P0', 'shovel', 'shovel.ogg')
    candidate('C_DrillStart', '钻机启动：真实录音起音', 'P0', 'workshop', 'workshop/workshop - drill long.wav', end=.55)
    candidate('C_DrillMotor_Loop', '钻机持续转动：真实录音', 'P0', 'workshop', 'workshop/workshop - drill long.wav', start=.8, end=4.3, loop=True)
    candidate('C_DrillStop', '钻机停止：真实录音尾段', 'P0', 'workshop', 'workshop/workshop - drill long.wav', start=4.3)
    candidate('C_DrillTexture_Loop', '钻机接触纹理备选：仍需叠加岩石颗粒', 'P0', 'workshop', 'workshop/workshop - drill4.wav', start=.7, end=4.3, loop=True)
    candidate('C_Machine_Loop', '机械底层备选：需排除背景杂音', 'P2', 'workshop', 'workshop/workshop - machine.wav', start=2, end=10, loop=True)
    for index in range(2):
        candidate(f'C_DrillIdleAlt_{index:02d}', f'钻机怠速：科幻机械备选 {index + 1}', 'P0', 'kenney_scifi', f'Audio/engineCircular_{index:03d}.ogg', loop=True)
    for index in range(3):
        candidate(f'C_JetThrust_{index:02d}_Loop', f'喷气持续推进候选 {index + 1}', 'P0', 'kenney_scifi', f'Audio/thrusterFire_{index:03d}.ogg', loop=True)
    candidate('C_JetStart', '喷气启动：推进起音候选', 'P0', 'kenney_scifi', 'Audio/thrusterFire_000.ogg', end=.4)
    for index in range(1, 4):
        candidate(f'C_JetVent_{index:02d}', f'喷气停止／过热泄压候选 {index}', 'P0', 'steam', f'steam hisses - Marker #{index}.wav')
    for index in range(3):
        candidate(f'C_BombCrunch_{index:02d}', f'炸弹破岩：CC0 备选 {index + 1}', 'P0', 'kenney_scifi', f'Audio/explosionCrunch_{index:03d}.ogg')
    candidate('C_BombLarge_BY', '爆炸重击备选：需 CC BY 3.0 署名', 'P2', 'explosions', 'explosions/explode.wav')
    candidate('C_BombSmall_BY', '小型爆炸备选：需 CC BY 3.0 署名', 'P2', 'explosions', 'explosions/explodemini.wav')
    candidate('C_BedrockContact', '不可挖基岩反馈：金属短击仅作占位候选', 'P0', 'kenney_scifi', 'Audio/impactMetal_000.ogg')
    for index in range(1, 4):
        candidate(f'C_FootWood_{index:02d}', f'卧室木板脚步候选 {index}', 'P0', 'steps', f'wood{index:02d}.ogg')
    for name, label in [('stone01', '岩石脚步'), ('mud02', '泥土脚步'), ('gravel', '碎石脚步')]:
        candidate('C_Foot_' + name, label, 'P0', 'steps', name + '.ogg')
    candidate('C_Amb_Wind_Loop', '竖井／普通矿道：低音量风声候选', 'P1', 'wind', 'wind.wav', start=5, end=30, stereo=True, loop=True)
    candidate('C_Amb_Cavern_Loop', '开阔溶洞／石笋区：洞穴空间候选', 'P1', 'dungeon', 'dungeon.ogg', start=10, end=40, stereo=True, loop=True)
    candidate('C_Amb_Drips_Loop', '溶洞／石笋区：稀疏滴水候选', 'P1', 'drips', 'drips.flac', stereo=True, loop=True)
    for index in range(2):
        candidate(f'C_Amb_NeuralAlt_{index:02d}_Loop', f'神经岩抽象共鸣备选 {index + 1}：非生物心跳', 'P2', 'kenney_scifi', f'Audio/forceField_{index:03d}.ogg', stereo=True, loop=True)
    candidate('C_Music_Cave_Preview', '溶洞探索音乐：Cave Theme（20秒节选）', 'P1', 'cave_music', 'cave_music.ogg', start=20, end=40, stereo=True)
    candidate('C_Music_DeepHaze_Preview', '后段推进音乐备选：Deep Haze（20秒节选）', 'P1', 'deep_haze', 'deep_haze.mp3', start=20, end=40, stereo=True)
    candidate('C_Music_EmptyCity_Preview', '偏暗风格备选：EmptyCity（20秒节选）', 'P2', 'empty_city', 'empty_city.ogg', start=20, end=40, stereo=True)
    for source_id in ['kenney_scifi', 'explosions']:
        source_path = OUTPUT / SOURCES[source_id]['file']
        if source_id == 'kenney_scifi':
            with zipfile.ZipFile(source_path) as archive:
                license_data = archive.read('License.txt')
        else:
            license_data = subprocess.check_output(['tar', '-xOf', str(source_path), 'explosions/info.txt'])
        (OUTPUT / 'Licenses' / (source_id + '_original.txt')).write_bytes(license_data)
    (OUTPUT / 'manifest.json').write_text(json.dumps(RECORDS, ensure_ascii=False, indent=2), encoding='utf-8')
    credits = ['MainGame audio candidates — acquired 2026-10-16',
               'Candidate sources only. These sounds are not yet imported into Unreal or selected for release.',
               'Changes: excerpts, channel conversion, 48 kHz resampling, DC removal, fades/crossfades and gain adjustment.',
               'Full details and source hashes: manifest.json / sources.json.', '']
    for source in SOURCES.values():
        credits.extend([source['title'] + ' — ' + source['author'], source['page_url'],
                        source['license'] + ': ' + source['license_url'], source['notes'], ''])
    credits.extend(['Existing AudioPilot sounds keep their original credits in ../AudioPilot/Licenses/CREDITS.txt.',
                    'If Deep Haze is shipped, include: Deep Haze — Kevin MacLeod (incompetech.com), CC BY 4.0.',
                    'If either Michel Baradari explosion is shipped, include author, source, CC BY 3.0 and modification notice.',
                    'Do not publish this entire candidate credit list as the final shipped asset list without reconciling actual usage.'])
    (OUTPUT / 'Licenses/CREDITS.txt').write_text('\n'.join(credits) + '\n', encoding='utf-8')
    build_catalog()
    summary = dict(downloads=len(SOURCES), downloaded_bytes=sum(source['bytes'] for source in SOURCES.values()),
                   candidates=len(RECORDS), sample_rate=RATE, clipped_samples=sum(record['clipped_samples'] for record in RECORDS),
                   checksum_validation='passed', decoding_validation='passed', output_validation='passed',
                   listening_review='not performed', main_game_runtime_test='not performed', unreal_import='not performed')
    (OUTPUT / 'validation.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == '__main__':
    main()
