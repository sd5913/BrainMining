"""Generate seven original short UI chimes; no external audio dependency."""
from pathlib import Path
import math, struct, wave
out=Path(__file__).resolve().parents[1]/'SourceAssets'/'Audio'/'BranchRewards'
out.mkdir(parents=True,exist_ok=True)
for name,notes in [('Skewer',[440,550,660]),('Aquarium',[523.25,783.99,1046.5]),('CoinBag',[659.25,880,1318.51]),('Cocktail',[587.33,739.99,1174.66]),('Taiji',[392,523.25,784]),('Subway',[440,659.25,880]),('Phone',[783.99,987.77,1174.66])]:
    samples=[]
    for i in range(44100):
        t=i/44100;v=0
        for n,f in enumerate(notes):
            age=t-n*.105
            if age>=0:
                envelope=min(1,age/.009)*math.exp(-age*7)*(1-min(1,t)**8)
                v+=envelope*(math.sin(2*math.pi*f*age)+.2*math.sin(2*math.pi*f*2.003*age))*.18
        samples.append(struct.pack('<h',round(max(-1,min(1,v))*28000)))
    with wave.open(str(out/f'S_Collect_{name}.wav'),'wb') as stream:
        stream.setparams((1,2,44100,0,'NONE','not compressed'));stream.writeframes(b''.join(samples))
