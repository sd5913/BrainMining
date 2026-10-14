"""Prepare the supplied transparent artwork for UE; originals remain unchanged."""
from pathlib import Path
from PIL import Image
import shutil

root = Path(__file__).resolve().parents[1]
out = root / 'SourceAssets/DrillEnergy'
out.mkdir(parents=True, exist_ok=True)
art = Image.open('D:/下载/G.png').convert('RGBA')
shutil.copy2('D:/下载/G.png', out / 'EnergyOriginal.png')
frame = art.copy()
# Keep the original outline, separate its eight existing rectangles from the fill.
frame.paste((0,0,0,0), (290,385,1320,635))
fill = Image.new('RGBA', art.size)
block = art.crop((305,390,418,629))
# Repeat the supplied rectangle to fill the whole storage area (ten cells).
for i in range(10):
    fill.alpha_composite(block, (305 + i*128,390))
bounds = art.getbbox()
for name, image in [('T_DrillEnergyFrame',frame),('T_DrillEnergyFill',fill)]:
    image.crop(bounds).transpose(Image.Transpose.ROTATE_90).save(out / (name+'.png'))

cursor = Image.open('D:/下载/G (2).png').convert('RGBA')
cursor_out = root / 'Content/Slate/Cursors'
cursor_out.mkdir(parents=True, exist_ok=True)
shutil.copy2('D:/下载/G (2).png', out / 'PickaxeCursorOriginal.png')
cursor = cursor.crop(cursor.getbbox())
# Nearest resampling preserves the pixel artwork. Hotspot is the upper pick tip.
for scale in [1,1.5,2]:
    side = round(40*scale)
    thumb = cursor.copy()
    thumb.thumbnail((side-4,side-4),Image.Resampling.NEAREST)
    square = Image.new('RGBA',(side,side))
    square.alpha_composite(thumb,(round(2*scale),round(2*scale)))
    suffix = '' if scale==1 else '@'+str(scale).rstrip('0').rstrip('.')+'x'
    square.save(cursor_out / ('PickaxeCursor'+suffix+'.png'))
print('Prepared energy outline/fill and three cursor resolutions')

warning = Image.open('C:/Users/Lenovo/AppData/Local/Temp/codex-clipboard-f3200c71-3530-46d2-b008-5078d964596b.png').convert('RGBA')
warning_out=root/'Art/Office/Popups'
warning_out.mkdir(parents=True,exist_ok=True)
warning.crop(warning.getbbox()).save(warning_out/'T_PendingWarning.png')
