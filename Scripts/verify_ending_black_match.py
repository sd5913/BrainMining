"""Read current rendered frames; never synthesizes or alters game screenshots."""
from pathlib import Path
from datetime import datetime, timezone
import json
from PIL import Image, ImageStat, ImageChops

project=Path(__file__).resolve().parents[1]
folder=project/'Saved/EndingEmergencePreview'
report_dir=project/'Saved/Validation/EndingPaperFinale'
run=json.loads((report_dir/'index.json').read_text(encoding='utf-8-sig'))
assert run['succeeded']==1 and run['failed']==0, 'Run the finale validation first'
test=run['tests'][0]
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
 assert dark>.025, f'Blank white interruption at sheet turn: {path.name}'
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
assert white>.45 and ink>.025, 'White paper / black printed dots missing'

front=read('paper-cave-front');screen=read('paper-screen-match')
assert front.size==screen.size
w,h=front.size;area=(int(w*.1),int(h*.1),int(w*.9),int(h*.85))
difference=ImageStat.Stat(ImageChops.difference(front.crop(area),screen.crop(area)))
mean_difference=max(difference.mean)/255
assert mean_difference<.065, f'Cave-to-monitor match changed: {mean_difference}'

result={'passed':True,'continuity_frames':frames,'exit_black_fraction':dark_fraction,
 'desktop_white_fraction':white,'desktop_ink_fraction':ink,'monitor_mean_difference':mean_difference}
(report_dir/'pixel-verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result))
