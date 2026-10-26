from PIL import Image
from pathlib import Path
import json
root=Path('SourceAssets/MuseumRefresh')
manifest=json.loads((root/'downloads.json').read_text())
for asset in ['concrete_cat_statue','rock_09','balloon','metal']:
 if asset in ('concrete_cat_statue','rock_09'):
  maps={x['kind']:Path(x['path']) for x in manifest if x['asset']==asset}
  rough=maps['roughness'];ao=None;metal=None
 elif asset=='balloon':
  rough=root/'balloon/textures/BalloonRough.png';ao=root/'balloon/textures/BalloonAO.png';metal=None
 else:
  rough=root/'metal/Metal032_2K-JPG_Roughness.jpg';ao=None;metal=root/'metal/Metal032_2K-JPG_Metalness.jpg'
 r=Image.open(rough).convert('L');a=Image.open(ao).convert('L').resize(r.size) if ao else Image.new('L',r.size,255)
 m=Image.open(metal).convert('L').resize(r.size) if metal else Image.new('L',r.size,0)
 Image.merge('RGB',(a,r,m)).save(root/asset/(asset+'_ORM.png'))
print('PBR_TEXTURES_READY')
