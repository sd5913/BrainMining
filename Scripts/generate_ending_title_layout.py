from PIL import Image,ImageDraw,ImageFont
from pathlib import Path
import math
root=Path(__file__).resolve().parents[1]
font=ImageFont.truetype(str(root/'SourceAssets/Fonts/SourceHanSansCN-Bold.otf'),300)
word='WOoOW';bbox=font.getbbox(word);width=math.ceil(font.getlength(word));height=bbox[3]-bbox[1]
mask=Image.new('L',(width,height));d=ImageDraw.Draw(mask);d.text((0,-bbox[1]),word,font=font,fill=255)
count=666
for step in range(50,2,-1):
    pts=[]
    for row,y in enumerate(range(step//2,height,round(step*.866))):
        for x in range(step//2+(row%2)*step//2,width,step):
            if mask.getpixel((x,y))>180:pts.append((x,y))
    if len(pts)>=count:break
pts=[pts[math.floor(i*len(pts)/count)] for i in range(count)]
ends=[];pos=0
for c in word:pos+=font.getlength(c);ends.append(pos)
lines=['#pragma once','// Source Han Sans CN Bold, sampled from SourceAssets/Fonts/SourceHanSansCN-Bold.otf.','// Regenerate with Scripts/generate_ending_title_layout.py.','namespace EndingTitleLayout {','struct FPoint { float Y,Z; int32 Glyph; };','inline constexpr FPoint Points[] = {']
preview=Image.new('RGB',(width+50,height+50),(20,18,26));draw=ImageDraw.Draw(preview)
for x,y in pts:
    glyph=next(i for i,e in enumerate(ends) if x<e)
    lines.append('{%.8ff,%.8ff,%d},'%((x-width/2)/width,(height/2-y)/height,glyph))
    draw.ellipse((x+25-9,y+25-9,x+25+9,y+25+9),fill=(220,135,160))
lines+=['};','}']
(root/'Source/BrainMining/EndingTitleLayout.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'Saved/Validation/Feedback16').mkdir(parents=True,exist_ok=True)
preview.save(root/'Saved/Validation/Feedback16/title_layout.png')
print(dict(points=len(pts),grid_step=step,width=width,height=height))
