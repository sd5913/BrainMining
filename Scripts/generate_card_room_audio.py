"""Original synthesized placeholder cues. Standard Python only; no third-party audio."""
import math,random,wave,struct,pathlib
root=pathlib.Path(__file__).resolve().parents[1]/"Saved"/"CardRoomSourceAudio"
root.mkdir(parents=True,exist_ok=True)
rate=24000
for name,duration in {"S_Laser":.65,"S_Poison":1.1,"S_Impact":.55,"S_Finale":1.35,"S_Arrival":.7,"S_Draw":.15,"S_Reward":1.2}.items():
 rng=random.Random(13); samples=[]
 for i in range(int(rate*duration)):
  t=i/rate; a=t/duration; noise=rng.uniform(-1,1)
  if name=="S_Laser": v=math.sin(2*math.pi*(1050*t-560*t*t))*.5+noise*.08
  elif name=="S_Poison": v=(math.sin(2*math.pi*68*t)+.45*math.sin(2*math.pi*134*t)+noise*.4)*(.45+.3*math.sin(t*55))
  elif name=="S_Impact": v=noise*.65+math.sin(2*math.pi*53*t)*.35
  elif name=="S_Finale": v=math.sin(2*math.pi*(120*t+250*t*t))*.35+noise*.3
  elif name=="S_Arrival": v=math.sin(2*math.pi*(360*t-210*t*t))*.4+noise*.2
  elif name=="S_Draw": v=noise*.3
  else:
   notes=[523.25,659.25,783.99,1046.5]; note=notes[min(3,int(a*4))]; v=math.sin(2*math.pi*note*t)*.5+math.sin(4*math.pi*note*t)*.1
  envelope=min(1,t/.025)*(1-a)**1.4
  samples.append(struct.pack('<h',int(max(-1,min(1,v*envelope*.65))*32767)))
 with wave.open(str(root/(name+'.wav')),'wb') as f:
  f.setparams((1,2,rate,0,'NONE','not compressed')); f.writeframes(b''.join(samples))
print(root)