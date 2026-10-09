"""Original synthesized stone impacts with a short cavern echo. No external samples."""
import math,random,wave,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'Saved'/'EchoPassageSourceAudio';root.mkdir(parents=True,exist_ok=True)
rate=24000
for name,duration in [('S_Knock',1.65),('S_Landing',2.1),('S_Crumble',1.1),('S_Exit',1.9)]:
    rng=random.Random(41);dry=[]
    for i in range(int(rate*duration)):
        t=i/rate;n=rng.uniform(-1,1)
        if name=='S_Exit':v=sum(math.sin(2*math.pi*f*t) for f in (220,277.18,329.63))*.13*min(1,t/.15)*math.exp(-t*1.8)
        elif name=='S_Crumble':v=n*.45*math.exp(-t*4)+math.sin(t*2*math.pi*61)*.2*math.exp(-t*5)
        else:
            low=68 if name=='S_Landing' else 240
            v=(n*.55*math.exp(-t*32)+math.sin(t*2*math.pi*low)*.4*math.exp(-t*16)+math.sin(t*2*math.pi*1180)*.13*math.exp(-t*25))*min(1,t/.002)
        dry.append(v)
    out=dry.copy()
    if name!='S_Exit':
        for delay,amp in [(.13,.38),(.27,.24),(.43,.15),(.64,.09),(.89,.05)]:
            offset=int(delay*rate)
            for i in range(offset,len(out)):out[i]+=dry[i-offset]*amp
    with wave.open(str(root/(name+'.wav')),'wb') as f:
        f.setparams((1,2,rate,0,'NONE','not compressed'));f.writeframes(b''.join(struct.pack('<h',int(max(-1,min(1,v))*.85*32767)) for v in out))
print(root)
