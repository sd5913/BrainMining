"""Encode an actual timestamped emergence review; keeps recorded wall-clock speed."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('item', choices=['pigeon', 'subway'])
parser.add_argument('--take', type=Path)
parser.add_argument('--ffmpeg', type=Path)
args = parser.parse_args()
take = args.take or Path((ROOT/'Saved/Review/EmergenceFix'/('latest-'+args.item+'.txt')).read_text(encoding='utf8').strip())
report = json.loads((take/'recording.json').read_text(encoding='utf8'))
assert report['passed'], report.get('error')
frames = report['frames']
assert frames
ffmpeg = args.ffmpeg or shutil.which('ffmpeg') or next((ROOT/'output').glob('**/imageio_ffmpeg/binaries/ffmpeg*.exe'), None)
assert ffmpeg
lines = []
for i, frame in enumerate(frames):
    lines.append("file 'frames/"+frame['file']+"'")
    duration = frames[i+1]['seconds']-frame['seconds'] if i+1<len(frames) else 1/30
    lines.append('duration %.6f' % max(.001, duration))
lines.append("file 'frames/"+frames[-1]['file']+"'")
(take/'frames.txt').write_text('\n'.join(lines)+'\n', encoding='utf8')
labels = dict(approach='实机定点验证：原有地图、独立测试存档',
              digging='肉鸽：正常镐击逐层挖掘加厚土层',
              flock='挖通出口后：鸟群朝玩家飞来',
              final_dig='继续挖掘：本体保持原位，逐渐露出',
              revealed='本体可拾取；没有跳出土层或位移',
              subway_dig='晚高峰：逐步切割真实土体，验证部分暴露',
              crowd_wait='达到动画暴露门槛：先看人潮涌出，约 3 秒后开放拾取',
              subway_ready='无需继续挖到 70%，现在已经可以拾取',
              subway_support='验证挖空支撑：移除车厢下方真实土体',
              subway_fall='失去支撑后垂直落地，没有向外弹出',
              subway_collect='正常 E 键拾取', subway_collected='已获得「晚高峰」')

def stamp(seconds):
    cs = round(seconds*100)
    return f'{cs//360000}:{cs//6000%60:02}:{cs//100%60:02}.{cs%100:02}'

ass = '''[Script Info]
ScriptType: v4.00+
PlayResX: 1280
PlayResY: 720
[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Microsoft YaHei,23,&H00FFFFFF,&H00FFFFFF,&HAA000000,&HAA000000,0,0,0,0,100,100,0,0,3,1,0,2,20,20,16,1
[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
'''
start = 0
for i in range(1, len(frames)+1):
    if i==len(frames) or frames[i]['stage']!=frames[start]['stage']:
        end = frames[i]['seconds'] if i<len(frames) else frames[-1]['seconds']+1/30
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
cmd += ['-movflags', '+faststart', args.item+'-review.mp4']
subprocess.run(cmd, cwd=take, check=True)
print(take/(args.item+'-review.mp4'))
