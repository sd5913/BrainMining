"""Technical resampling of unmodified NASA Black Marble raster data for UE.

Run with Pillow. Keep originals and geographic coordinates; no synthetic lights.
"""
from pathlib import Path
from PIL import Image

Image.MAX_IMAGE_PIXELS = 500_000_000
root = Path(__file__).resolve().parents[1] / 'SourceAssets/EndingEmergence/ThirdParty/NASA'
with Image.open(root / 'BlackMarble_2016_3km_gray.jpg') as source:
    assert source.size == (13500, 6750)
    source.convert('L').resize((8192, 4096), Image.Resampling.LANCZOS).save(root / 'T_NASA_NightGlobal.png')
# B1: -90..0 east, C1: 0..90 east; both 90..0 north.
# A continuous -22.5..67.5 degree detail tile spanning Europe and the Mediterranean.
detail = Image.new('L', (21600, 21600))
with Image.open(root / 'BlackMarble_2016_B1_gray.jpg') as source:
    assert source.size == (21600, 21600)
    detail.paste(source.crop((16200, 0, 21600, 21600)).convert('L'), (0, 0))
with Image.open(root / 'BlackMarble_2016_C1_gray.jpg') as source:
    assert source.size == (21600, 21600)
    detail.paste(source.crop((0, 0, 16200, 21600)).convert('L'), (5400, 0))
detail.resize((8192, 8192), Image.Resampling.LANCZOS).save(root / 'T_NASA_NightEurope.png')
print('Prepared 8192x4096 global / 8192x8192 European night-light textures.')
