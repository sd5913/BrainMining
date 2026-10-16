"""Read real rendered screenshots; never synthesizes or alters the images."""
from pathlib import Path
import json
import re
from PIL import Image, ImageStat

project=Path(__file__).resolve().parents[1]
log=(project/'Saved/Logs/EndingPaperFinaleTest.log').read_text(encoding='utf-8',errors='replace')
counts=re.findall(r'Black match: (\d+) screenshot probes requested',log)
assert counts, 'Run TitleDrillReality with the paper finale validation launcher first'
count=int(counts[-1]);assert count>=5
folder=project/'Saved/EndingEmergencePreview'
frames=[]
for i in range(count):
 path=folder/f'paper-black-match-{i:02d}.png'
 im=Image.open(path).convert('RGB');w,h=im.size
 # Exclude only the lower-left R hint and the extreme framing edge.
 region=im.crop((int(w*.05),int(h*.05),int(w*.95),int(h*.85)))
 stat=ImageStat.Stat(region)
 mean=max(stat.mean)/255
 peak=max(high for low,high in stat.extrema)/255
 frames.append({'file':path.name,'size':[w,h],'mean':mean,'peak':peak})
 assert mean<.015 and peak<.10, f'Bright exposure during the black-screen match: {path.name}: {mean}, {peak}'

portal=Image.open(folder/'paper-luminous-portal.png').convert('RGB');w,h=portal.size
centre=ImageStat.Stat(portal.crop((int(w*.4),int(h*.4),int(w*.6),int(h*.6))))
portal_light=min(centre.mean)/255
assert portal_light>.72, f'Tunnel exit should be luminous white: {portal_light}'
flight=Image.open(folder/'paper-black-flight.png').convert('RGB');w,h=flight.size
dark=flight.crop((int(w*.2),int(h*.2),int(w*.8),int(h*.75))).convert('L').histogram()
dark_fraction=sum(dark[:35])/sum(dark)
assert dark_fraction>.85, f'Black-space flight is obscured by scenery: {dark_fraction}'
centre_peak=max(high for low,high in ImageStat.Stat(flight.crop((int(w*.47),int(h*.45),int(w*.53),int(h*.55)))).extrema)/255
assert centre_peak>.85, 'Distant white aperture missing in the black flight'
desk=Image.open(folder/'paper-flight-middle.png').convert('RGB');w,h=desk.size
ground=ImageStat.Stat(desk.crop((int(w*.35),int(h*.72),int(w*.65),int(h*.88))))
ground_white=min(ground.mean)/255;ground_tint=(max(ground.mean)-min(ground.mean))/255
assert ground_white>.80 and ground_tint<.03, f'Desktop must remain neutral white: {ground.mean}'
report={'passed':True,'black_frames':frames,'white_portal_min_channel_mean':portal_light,'exit_black_fraction':dark_fraction,'desktop_white':ground_white,'desktop_tint':ground_tint}
out=project/'Saved/Validation/EndingPaperFinale/pixel-verification.json'
out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'passed':True,'frames':count,'black_peak':max(f['peak'] for f in frames),'white_portal':portal_light}))
