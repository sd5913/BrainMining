"""Typeset the final sheet's ink mask from the unmodified OFL Xiaolai font.

Run with Pillow before the Unreal asset generator. This only creates typography;
the downloaded paper and water photographs stay unmodified.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1] / 'SourceAssets/EndingEmergence/ThirdParty'
font = ImageFont.truetype(str(root / 'Xiaolai-Regular.ttf'), 290)
ink = Image.new('RGB', (2048, 1024), 'black')
draw = ImageDraw.Draw(ink)
text = '挖哦！'
box = draw.textbbox((0, 0), text, font=font, stroke_width=2)
draw.text(((2048-box[2]-box[0])/2, (1024-box[3]-box[1])/2), text,
          font=font, fill='white', stroke_width=2, stroke_fill='white')
ink.save(root / 'EndingHandwrittenInk.png')
