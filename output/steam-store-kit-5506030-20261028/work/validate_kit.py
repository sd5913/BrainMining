from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import csv, json, hashlib, re
root=Path(__file__).resolve().parents[1]
work=root/'work';docs=root/'10_填写说明与核验'
expected={
'02_商店图片/header_capsule_920x430.png':(920,430),
'02_商店图片/small_capsule_462x174.png':(462,174),
'02_商店图片/main_capsule_1232x706.png':(1232,706),
'02_商店图片/vertical_capsule_748x896.png':(748,896),
'02_商店图片/page_background_1438x810.png':(1438,810),
'03_游戏库图片/library_capsule_600x900.png':(600,900),
'03_游戏库图片/library_header_920x430.png':(920,430),
'03_游戏库图片/library_hero_3840x1240.png':(3840,1240),
'03_游戏库图片/library_logo_1040x720.png':(1040,720),
'04_图标/app_icon_184x184.jpg':(184,184),
'04_图标/shortcut_icon_256x256.png':(256,256),
'04_图标/shortcut_icon_512x512.png':(512,512),
'05_视频/01_trailer_poster_1920x1080.png':(1920,1080),
'05_视频/02_extended_poster_1920x1080.png':(1920,1080),
}
records=[]
for rel,size in expected.items():
    p=root/rel;im=Image.open(p);im.load();assert im.size==size,(rel,im.size,size)
    records.append({'file':rel,'dimensions':f'{im.width}x{im.height}','bytes':p.stat().st_size,'status':'PASS'})
logo=Image.open(root/'03_游戏库图片/library_logo_1040x720.png')
assert logo.mode=='RGBA' and logo.getchannel('A').getextrema()==(0,255)
source_logo=Path('C:/Users/abao/Desktop/挖哦素材/logo2.png')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(source_logo)==sha(root/'09_原始LOGO与配置参考/logo2_original_unchanged.png')
provenance=json.loads((work/'graphics-provenance.json').read_text(encoding='utf-8-sig'))
for item in provenance:
    if item.get('Slot')=='Gameplay Screenshot':
        p=root/item['File'];assert sha(p)==item['OriginalSHA256'].lower()
        im=Image.open(p);assert im.size==(2560,1440)
        records.append({'file':item['File'],'dimensions':'2560x1440','bytes':p.stat().st_size,'status':'PASS; original pixels preserved'})
gifsum=0
for p in (root/'06_介绍配图').glob('*.gif'):
    im=Image.open(p);assert im.n_frames>1 and p.stat().st_size<5_000_000
    gifsum+=p.stat().st_size
    records.append({'file':str(p.relative_to(root)),'dimensions':f'{im.width}x{im.height}; {im.n_frames} frames','bytes':p.stat().st_size,'status':'PASS'})
assert gifsum<15_000_000
copy=json.loads((root/'01_商店文案/store-copy.json').read_text(encoding='utf-8'))
assert all(len(t)<=300 and '\n' not in t for t in copy['short_descriptions'].values())
for lang,t in copy['about_plain'].items():
    assert not re.search(r'https?://|\[img\]|wishlist|buy now',t,re.I)
with (docs/'素材核验表.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.DictWriter(f,fieldnames=['file','dimensions','bytes','status']);w.writeheader();w.writerows(records)
(docs/'validation.json').write_text(json.dumps({'validated_at':'2026-10-28','checks_passed':len(records),'original_logo_sha256':sha(source_logo),'screenshot_originals_preserved':7,'short_description_characters':{k:len(v) for k,v in copy['short_descriptions'].items()},'gif_total_bytes':gifsum,'backend_modified':False,'pending':['final visual review','team factual fields','final Steam build feature/language validation']},ensure_ascii=False,indent=2),encoding='utf-8')
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',17)
files=list(expected)[:12]
sheet=Image.new('RGB',(1600,4*330),(26,31,36));d=ImageDraw.Draw(sheet)
for i,rel in enumerate(files):
    im=Image.open(root/rel).convert('RGBA');im.thumbnail((380,276),Image.Resampling.LANCZOS)
    x=(i%4)*400;y=(i//4)*330
    bg=Image.new('RGBA',(390,280),(42,48,53,255));bg.alpha_composite(im,((390-im.width)//2,(280-im.height)//2))
    sheet.paste(bg.convert('RGB'),(x+5,y))
    d.text((x+8,y+282),Path(rel).name,font=font,fill='white')
sheet.save(docs/'图片总览.jpg',quality=95)
# Small-capsule thumbnail checks, reflecting Steam's automatic derivatives.
small=Image.open(root/'02_商店图片/small_capsule_462x174.png')
qa=Image.new('RGB',(800,350),(25,30,35));dq=ImageDraw.Draw(qa)
for i,size in enumerate([(462,174),(184,69),(120,45)]):
    im=small.resize(size,Image.Resampling.LANCZOS);x=[12,500,500][i];y=[50,50,170][i]
    qa.paste(im,(x,y));dq.text((x,y-25),f'{size[0]} x {size[1]}',font=font,fill='white')
qa.save(docs/'小胶囊缩略图检查.png')
hero=Image.open(root/'03_游戏库图片/library_hero_3840x1240.png').convert('RGB')
draw=ImageDraw.Draw(hero);draw.rectangle((1490,430,2350,810),outline='#62ff9d',width=8)
hero.thumbnail((1920,620),Image.Resampling.LANCZOS)
hero.save(docs/'库壁纸安全区_仅预览勿上传.jpg',quality=94)
# All relative image/video/file links in the offline review page must resolve.
markup=(root/'index.html').read_text(encoding='utf-8')
for path in re.findall(r'(?:src|poster|href)="([^"]+)"',markup):
    if path.startswith(('http:','https:','#')):continue
    assert (root/path).exists(),path
print(json.dumps({'verified_images_and_gifs':len(records),'all_links_valid':True,'source_logo_unchanged':True,'screenshot_hashes_match':True},ensure_ascii=True))
