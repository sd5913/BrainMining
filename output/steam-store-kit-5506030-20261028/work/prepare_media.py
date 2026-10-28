import sys, subprocess, shutil, json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(Path(__file__).parent/'video-tools'))
import imageio_ffmpeg
ff=imageio_ffmpeg.get_ffmpeg_exe()
src=Path('C:/Users/abao/Desktop/挖哦素材')
videos=root/'05_视频'; videos.mkdir(exist_ok=True)
gifs=root/'06_介绍配图'; gifs.mkdir(exist_ok=True)
work=Path(__file__).parent
log=open(work/'video-prepare.log','w',encoding='utf-8')
def run(args):
    p=subprocess.run([ff,'-hide_banner','-nostdin','-y']+args,stdout=log,stderr=log)
    if p.returncode: raise RuntimeError('ffmpeg failed; see work/video-prepare.log')
shutil.copy2(src/'宣传片.mp4',videos/'01_gameplay_trailer_1920x1080_30fps.mp4')
run(['-i',str(src/'实机.mp4'),'-map','0:v:0','-map','0:a:0?','-c:v','libx264','-preset','fast','-crf','18','-maxrate','16M','-bufsize','32M','-pix_fmt','yuv420p','-r','30','-c:a','aac','-b:a','192k','-ac','2','-ar','48000','-movflags','+faststart',str(videos/'02_extended_gameplay_1920x1080_30fps.mp4')])
for name,source,t in [('01_trailer_poster_1920x1080.png','宣传片.mp4',3),('02_extended_poster_1920x1080.png','实机.mp4',65)]:
    run(['-ss',str(t),'-i',str(src/source),'-frames:v','1',str(videos/name)])
for name,source,t in [('01_digging','宣传片.mp4',3),('02_office','实机.mp4',16)]:
    palette=work/f'{name}-palette.png'
    vf='fps=8,scale=616:-2:flags=lanczos'
    run(['-ss',str(t),'-t','4','-i',str(src/source),'-vf',vf+',palettegen=max_colors=128:stats_mode=diff','-frames:v','1',str(palette)])
    run(['-ss',str(t),'-t','4','-i',str(src/source),'-i',str(palette),'-lavfi',vf+'[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle','-loop','0',str(gifs/(name+'.gif'))])
metadata={}
for f in sorted(videos.glob('*.mp4')):
    p=subprocess.run([ff,'-hide_banner','-i',str(f)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    metadata[f.name]=[l.strip() for l in p.stderr.splitlines() if 'Duration:' in l or 'Stream #' in l]
(videos/'video-metadata.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({f.name:f.stat().st_size for f in list(videos.iterdir())+list(gifs.iterdir())},ensure_ascii=True,indent=2))
log.close()
