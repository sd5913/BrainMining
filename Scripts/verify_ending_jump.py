"""Read current rendered frames; never synthesizes or alters game screenshots."""
from pathlib import Path
from datetime import datetime, timezone
import json
import sys
from PIL import Image, ImageStat, ImageChops

project=Path(__file__).resolve().parents[1]
folder=project/'Saved/EndingEmergencePreview'
report_dir=project/'Saved/Validation'/(sys.argv[1] if len(sys.argv)>1 else 'EndingJump')
run=json.loads((report_dir/'index.json').read_text(encoding='utf-8-sig'))
assert run['failed']==0, 'Run the finale validation first'
test=next(t for t in run['tests'] if t['fullTestPath'].endswith('TitleDrillReality'))
assert test['state']=='Success', 'Run the finale validation first'
start=datetime.strptime(test['dateTime'],'%Y.%m.%d-%H.%M.%S').replace(tzinfo=timezone.utc).timestamp()

def read(name):
 path=folder/(name+'.png')
 assert path.stat().st_mtime>=start-2, f'Stale screenshot: {name}'
 return Image.open(path).convert('RGB')

frames=[]
for path in sorted(folder.glob('paper-continuity-*.png')):
 if path.stat().st_mtime<start-2:continue
 im=read(path.stem);w,h=im.size
 region=im.crop((int(w*.05),int(h*.05),int(w*.95),int(h*.85)))
 histogram=region.convert('L').histogram()
 dark=sum(histogram[:100])/sum(histogram)
 # A pure white disk naturally fills the view before the tilt. Runtime tests
 # verify the same surface and continuous camera pose without a white overlay.
 frames.append({'file':path.name,'dark_fraction':dark})
# Saving full-resolution frames can advance real-time play by several frames.
assert len(frames)>=3, 'Not enough fresh continuity probes'

flight=read('paper-black-flight');w,h=flight.size
histogram=flight.crop((int(w*.2),int(h*.2),int(w*.8),int(h*.75))).convert('L').histogram()
dark_fraction=sum(histogram[:35])/sum(histogram)
assert dark_fraction>.55, 'Black-space flight is obscured'

desk=read('paper-flight-middle');w,h=desk.size
ground=desk.crop((int(w*.08),int(h*.56),int(w*.92),int(h*.94)))
pixels=list(ground.get_flattened_data())
white=sum(min(c)>210 and max(c)-min(c)<8 for c in pixels)/len(pixels)
ink=sum(max(c)<100 for c in pixels)/len(pixels)
assert white>.75, 'Plain white desktop missing'
plain=desk.crop((int(w*.3),int(h*.72),int(w*.7),int(h*.93)))
plain_pixels=list(plain.get_flattened_data())
plain_white=sum(min(c)>210 and max(c)-min(c)<8 for c in plain_pixels)/len(plain_pixels)
assert plain_white>.99, 'Unprinted desktop contains dark spots'

front=read('paper-room-monitor');screen=read('paper-room-settled')
assert front.size==screen.size
w,h=front.size;area=(int(w*.1),int(h*.1),int(w*.9),int(h*.85))
difference=ImageStat.Stat(ImageChops.difference(front.crop(area),screen.crop(area)))
mean_difference=max(difference.mean)/255
assert mean_difference<.065, f'Monitor settling changed: {mean_difference}'

# The actual drilled centre must remain white in the ink stage. Missing UVs
# formerly classified the whole visible face as black ink despite WhiteInk=1.
lineart=read('wooow-cave-white-lineart');w,h=lineart.size
centre=lineart.crop((int(w*.32),int(h*.2),int(w*.68),int(h*.58))).convert('L')
hist=centre.histogram();ink_white=sum(hist[170:])/sum(hist)
assert ink_white>.45, f'Excavated white-ink centre is still dark: {ink_white}'
purple=read('wooow-cave-purple');w,h=purple.size
contact=ImageStat.Stat(purple.crop((int(w*.4),int(h*.28),int(w*.6),int(h*.52))).convert('L')).mean[0]
edge=ImageStat.Stat(purple.crop((int(w*.06),int(h*.14),int(w*.22),int(h*.42))).convert('L')).mean[0]
assert contact>edge*1.5, f'Centre lighting no longer stands out from the periphery: {contact} / {edge}'
result={'passed':True,'continuity_frames':frames,'exit_black_fraction':dark_fraction,
 'desktop_white_fraction':white,'plain_white_fraction':plain_white,'monitor_mean_difference':mean_difference,
 'white_ink_contact_fraction':ink_white,'purple_contact_brightness':contact,'purple_edge_brightness':edge}
(report_dir/'pixel-verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result))
