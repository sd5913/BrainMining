"""Extract the six supplied popup skins, trimming transparent outer padding only."""
from PIL import Image,ImageDraw,ImageFont
import pathlib,zipfile,json
root=pathlib.Path(__file__).resolve().parents[1];out=root/'Art/Office/Popups';out.mkdir(parents=True,exist_ok=True)
names=['Window','SystemMessage','SystemError','Robot','Message','Loading'];manifest=[]
with zipfile.ZipFile('D:/下载/G.zip') as z:
 for i,name in enumerate(names):
  source=z.read(str(i+5)+'.png');(out/(name+'_Original.png')).write_bytes(source)
  im=Image.open(out/(name+'_Original.png')).convert('RGBA');bounds=im.getbbox();trim=im.crop(bounds);trim.save(out/('T_Popup'+name+'.png'))
  manifest.append({'name':name,'source':str(i+5)+'.png','original_size':list(im.size),'trim':list(bounds),'size':list(trim.size)})
  if i<3:
   # Floor / airborne copies use printed text rather than hundreds of extra world widgets.
   texts=[('DDL 提醒','今日 18:00 前提交项目汇报。\n附件、数据和演示稿仍待确认。'),('有新的工作消息','项目群：刚才的方案需要再调整一下。\n请尽快回复并同步最新进度。'),('新增待办','你收到一项新的紧急任务。\n优先级：高。交付时间：今天。')]
   rects=[(.07,.27,.86,.4),(.065,.24,.87,.4),(.18,.22,.76,.39)];sizes=[(300,302),(420,257),(520,177)];r=rects[i];w,h=trim.size;s=w/sizes[i][0]
   physical=trim.copy();draw=ImageDraw.Draw(physical);titlefont=ImageFont.truetype('C:/Windows/Fonts/simhei.ttf',int(18*s));font=ImageFont.truetype('C:/Windows/Fonts/simhei.ttf',int((15 if i==2 else 17)*s));x,y=int(r[0]*w),int(r[1]*h);draw.text((x,y),texts[i][0],font=titlefont,fill=(12,12,14,255));y+=int(27*s)
   for paragraph in texts[i][1].split('\n'):
    line=''
    for ch in paragraph:
     if draw.textlength(line+ch,font=font)>r[2]*w and line:
      draw.text((x,y),line,font=font,fill=(12,12,14,255));y+=int((18 if i==2 else 21)*s);line=''
     line+=ch
    if line:draw.text((x,y),line,font=font,fill=(12,12,14,255));y+=int((18 if i==2 else 21)*s)
   physical.save(out/('T_PhysicalPopup'+name+'.png'))
(out/'Manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8');print(manifest)
