"""Regenerate the chamfered menu brushes and fit the supplied transparent title."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageOps
root=Path(__file__).resolve().parents[1];out=root/'SourceAssets/UI/MenuStyle';out.mkdir(parents=True,exist_ok=True)
for name,size,color,stroke,cut in [('Panel',(1200,900),(18,17,14,244),(242,175,18,235),24),('Row',(800,64),(8,8,6,220),(94,80,48,140),12),('Hover',(800,64),(255,182,0,255),(255,213,87,255),12),('Pressed',(800,64),(198,137,0,255),(255,213,87,255),12)]:
    w,h=size;im=Image.new('RGBA',size);d=ImageDraw.Draw(im);pts=[(1,1),(w-cut-1,1),(w-2,cut),(w-2,h-2),(cut,h-2),(1,h-cut)]
    d.polygon(pts,fill=color);d.line(pts+[pts[0]],fill=stroke,width=2)
    if name=='Panel':d.line((28,4,w-cut-20,4),fill=(255,191,22,255),width=3)
    im.save(out/f'T_Menu{name}.png')
im=Image.open(root/'SourceAssets/Opening/TitleSupplied20261020.png');alpha=im.getchannel('A');w,h=im.size
rows=[y for y in range(h) if sum(v>20 for v in alpha.crop((0,y,w,y+1)).getdata())>12]
cols=[x for x in range(w) if sum(v>20 for v in alpha.crop((x,0,x+1,h)).getdata())>12]
im=im.crop((max(0,min(cols)-12),max(0,min(rows)-12),min(w,max(cols)+13),min(h,max(rows)+13)))
im=ImageOps.contain(im,(1024,600),Image.Resampling.LANCZOS);canvas=Image.new('RGBA',(1024,600));canvas.alpha_composite(im,((1024-im.width)//2,(600-im.height)//2));canvas.save(root/'SourceAssets/Opening/TitleTransparent.png')
