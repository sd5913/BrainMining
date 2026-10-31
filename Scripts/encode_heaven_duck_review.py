"""Encode a timestamped PIE capture at its original speed, with review captions."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--take', type=Path)
parser.add_argument('--ffmpeg', type=Path)
args = parser.parse_args()
take = args.take or Path((ROOT/'Saved/Review/HeavenDuckGush/latest-take.txt').read_text(encoding='utf8').strip())
report = json.loads((take/'recording.json').read_text(encoding='utf8'))
frames = report['frames']
assert frames, 'No captured frames'
ffmpeg = args.ffmpeg or shutil.which('ffmpeg')
if not ffmpeg:
    ffmpeg = next((ROOT/'output').glob('**/imageio_ffmpeg/binaries/ffmpeg*.exe'), None)
assert ffmpeg, 'Supply --ffmpeg with an installed ffmpeg executable'
lines = []
for i, frame in enumerate(frames):
    lines.append("file 'frames/"+frame['file']+"'")
    duration = frames[i+1]['seconds']-frame['seconds'] if i+1 < len(frames) else 1/30
    lines.append('duration %.6f' % max(.001, duration))
lines.append("file 'frames/"+frames[-1]['file']+"'")
(take/'frames.txt').write_text('\n'.join(lines)+'\n', encoding='utf8')

labels = {
    'ambient': '远处：保持原有数量和频率',
    'approach': '靠近马桶：触发定向密集喷涌',
    'gush_cycles': '单次喷涌 10 秒：玩家左右移动，喷射方向缓慢跟随',
    'gush_pause': '暂停 2.5 秒，然后继续喷涌；落地鸭子自动回收',
    'gush_settle': '玩家逐渐停下：喷射方向平滑追上目标位置',
    'leave': '离开触发范围',
    'ambient_return': '恢复原有喷鸭节奏',
}


def stamp(seconds):
    cs = round(seconds*100)
    return f'{cs//360000}:{cs//6000%60:02}:{cs//100%60:02}.{cs%100:02}'


ass = '''[Script Info]
ScriptType: v4.00+
PlayResX: 1280
PlayResY: 720
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Microsoft YaHei,24,&H00FFFFFF,&H00FFFFFF,&HAA000000,&HAA000000,0,0,0,0,100,100,0,0,3,1,0,2,20,20,20,1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
'''
start = 0
for i in range(1, len(frames)+1):
    if i == len(frames) or frames[i]['stage'] != frames[start]['stage']:
        end = frames[i]['seconds'] if i < len(frames) else frames[-1]['seconds']+1/30
        ass += f"Dialogue: 0,{stamp(frames[start]['seconds'])},{stamp(end)},Default,,0,0,0,,{labels[frames[start]['stage']]}\n"
        start = i
(take/'captions.ass').write_text(ass, encoding='utf-8-sig')
cmd = [str(ffmpeg), '-hide_banner', '-y', '-f', 'concat', '-safe', '0', '-i', 'frames.txt']
audio = take/'live-audio.wav'
if audio.exists():
    cmd += ['-i', audio.name]
cmd += ['-vf', 'subtitles=captions.ass,pad=ceil(iw/2)*2:ceil(ih/2)*2',
        '-r', '30', '-c:v', 'libx264', '-preset', 'medium', '-crf', '19', '-pix_fmt', 'yuv420p']
if audio.exists():
    cmd += ['-c:a', 'aac', '-b:a', '160k', '-shortest']
cmd += ['-movflags', '+faststart', 'heaven-duck-gush.mp4']
subprocess.run(cmd, cwd=take, check=True)
print(take/'heaven-duck-gush.mp4')
