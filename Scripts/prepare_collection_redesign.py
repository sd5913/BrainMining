"""Keep supplied artwork intact and crop transparent padding for editable UMG images."""
from pathlib import Path
import shutil
import sys
from PIL import Image

root=Path(__file__).resolve().parents[1]
source=Path(sys.argv[1]) if len(sys.argv)>1 else Path('H:/BaiduNetdiskDownload/G (3)')
original=root/'SourceAssets/CollectionInspect/Original'
cropped=root/'SourceAssets/CollectionInspect/Cropped'
original.mkdir(parents=True,exist_ok=True);cropped.mkdir(parents=True,exist_ok=True)
for number,name in [(16,'Unknown'),(17,'Info'),(18,'Selected'),(19,'Arrow'),(20,'Back')]:
    supplied=source/f'{number}.png'
    shutil.copy2(supplied,original/supplied.name)
    image=Image.open(supplied).convert('RGBA')
    bounds=image.getchannel('A').getbbox()
    assert bounds,f'No artwork in {supplied}'
    image.crop(bounds).save(cropped/f'T_Collection{name}.png')
    print(name,bounds)
Image.new('RGBA',(4,4),(0,0,0,255)).save(cropped/'T_CollectionPreviewDefault.png')
