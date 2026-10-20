from pathlib import Path

from PIL import Image, ImageOps


root = Path(__file__).resolve().parents[1]
source = root / 'SourceAssets/Branding/ApplicationIcon.png'
sizes = {'drawable': 192, 'drawable-ldpi': 36, 'drawable-mdpi': 48,
         'drawable-hdpi': 72, 'drawable-xhdpi': 96,
         'drawable-xxhdpi': 144, 'drawable-xxxhdpi': 192}
with Image.open(source) as image:
    artwork = image.convert('RGBA')
    for folder, size in sizes.items():
        target = root / 'Build/Android/res' / folder / 'icon.png'
        target.parent.mkdir(parents=True, exist_ok=True)
        ImageOps.contain(artwork, (size, size), Image.Resampling.LANCZOS).save(target)
        print(f'{target.relative_to(root)}: {size}px')
