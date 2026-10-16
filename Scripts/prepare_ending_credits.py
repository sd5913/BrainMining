"""Generate the authored credit typography, using the locally installed CJK font."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
out=root/'SourceAssets/EndingEmergence/PaperFinale';out.mkdir(parents=True,exist_ok=True)
im=Image.new('RGB',(2048,1200),(4,8,17));d=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',145)
for name,y in [('Seven',260),('Rasimet',480),('空空道人',700),('铁皮辣妹',920)]:
 d.text((1024,y),name,fill=(233,238,237),font=font,anchor='mm')
d.line((690,1070,1358,1070),fill=(68,161,179),width=4)
im.save(out/'T_EE_DeveloperCredits.png')
