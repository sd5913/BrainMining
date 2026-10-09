"""Split the supplied transparent GUI PNGs into independently editable UMG layers.
Run with the workspace Python (Pillow), before import_mining_gui.py in Unreal.
The original ZIP is never modified. No drawing or AI replacement of the supplied art.
"""
import argparse
import math
from pathlib import Path
import zipfile
from PIL import Image

parser = argparse.ArgumentParser()
parser.add_argument('zip', type=Path)
args = parser.parse_args()
destination = Path(__file__).resolve().parents[1] / 'SourceAssets' / 'MiningGUI'
destination.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.zip) as archive:
    sources = {Path(n).stem: Image.open(archive.open(n)).convert('RGBA')
               for n in archive.namelist() if n.lower().endswith('.png')}

def save(name, image):
    image.save(destination / (name + '.png'))

def masked(image, predicate):
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            if not predicate(x, y, pixels[x, y]):
                pixels[x, y] = (0, 0, 0, 0)
    return result

for key, normal, used in [('G', '8', '17'), ('V', '9', '18')]:
    save('T_Ability' + key, sources[normal].crop((216, 114, 876, 774)))
    save('T_Ability' + key + 'Used', sources[used].crop(sources[used].getbbox()))
    keycap = sources[normal].crop((0, 821, 1080, 990))
    save('T_Key' + key, keycap.crop(keycap.getbbox()))

ring_box = (240, 122, 840, 722)
ring = sources['13'].crop(ring_box)
# Ring grey and white centre art are separate, preserving their original pixels.
save('T_RingGrey', masked(ring, lambda x, y, c: math.hypot(x-300, y-299) > 210))
for name, number in [('Pickaxe', '13'), ('Vortex', '14'), ('Blast', '15')]:
    save('T_Icon' + name, masked(sources[number].crop(ring_box),
                               lambda x, y, c: math.hypot(x-300, y-299) <= 210))
locked = sources['16'].crop(ring_box)
save('T_RingLocked', masked(locked, lambda x, y, c: math.hypot(x-300, y-299) > 210))
save('T_IconLocked', masked(locked, lambda x, y, c: math.hypot(x-300, y-299) <= 210))

# Colour each existing segment; geometry/antialiasing remain exactly the artist's.
for index in range(3):
    layer = ring.copy()
    pixels = layer.load()
    for y in range(600):
        for x in range(600):
            angle = math.degrees(math.atan2(y-299, x-300)) % 360
            segment = 0 if 150 <= angle < 270 else 1 if angle >= 270 or angle < 30 else 2
            r, g, b, a = pixels[x, y]
            if math.hypot(x-300, y-299) > 210 and segment == index:
                pixels[x, y] = (168, 255, 0, a)
            else:
                pixels[x, y] = (0, 0, 0, 0)
    save('T_RingGreen' + str(index+1), layer)

price = sources['13'].crop((0, 794, 1080, 949))
save('T_PriceButton', price.crop(price.getbbox()))
save('T_CoinBar', sources['10'].crop(sources['10'].getbbox()))
save('T_Plus', sources['12'].crop(sources['12'].getbbox()))
print('Prepared', len(list(destination.glob('*.png'))), 'independent layers in', destination)
