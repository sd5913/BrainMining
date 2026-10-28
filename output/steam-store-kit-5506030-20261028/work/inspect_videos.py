import sys, subprocess, re, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent/'video-tools'))
import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parent
src=Path('C:/Users/abao/Desktop/挖哦素材')
ff=imageio_ffmpeg.get_ffmpeg_exe()
meta={}
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',16)
sheet=Image.new('RGB',(1600,920),(25,25,25))
draw=ImageDraw.Draw(sheet)
for row,name in enumerate(['实机.mp4','宣传片.mp4']):
    p=subprocess.run([ff,'-hide_banner','-i',str(src/name)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    meta[name]=p.stderr
    for col,t in enumerate([3,15,35,65]):
        out=root/f'video-{row}-{t}.jpg'
        subprocess.run([ff,'-hide_banner','-loglevel','error','-ss',str(t),'-i',str(src/name),'-frames:v','1','-q:v','2','-y',str(out)],check=True)
        im=Image.open(out); im.thumbnail((394,420))
        x=col*400;y=row*460
        sheet.paste(im,(x+(400-im.width)//2,y))
        draw.text((x+6,y+425),f'{name} {t}s',font=font,fill='white')
sheet.save(root/'video-contact.jpg',quality=94)
(root/'video-source-metadata.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({name:[l for l in txt.splitlines() if 'Duration:' in l or 'Stream #' in l] for name,txt in meta.items()},ensure_ascii=False,indent=2))
