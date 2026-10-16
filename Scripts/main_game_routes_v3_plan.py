"""Regional route identities, in Blender metres. Shared mouth collars stay intact."""
import json,math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'Art/MainGame'
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']
STYLES=['narrow-branches','ascending-caverns','descending-spiral','mixed-expedition']
END_RISE_M=100.

def sample(points,step=.35):
 p=np.asarray(points,float);d=np.r_[0,np.cumsum(np.linalg.norm(np.diff(p,axis=0),axis=1))];keep=np.r_[True,np.diff(d)>1e-6];p=p[keep];d=d[keep];s=np.linspace(0,d[-1],math.ceil(d[-1]/step)+1)
 return np.column_stack([np.interp(s,d,p[:,i]) for i in range(3)]),s

def spline(ctrl):
 c=np.asarray(ctrl,float);pad=np.vstack([2*c[0]-c[1],c,2*c[-1]-c[-2]]);result=[]
 for k in range(len(c)-1):
  a,b,d,e=pad[k:k+4]
  for t in np.linspace(0,1,40,endpoint=False):result.append(.5*(2*b+(-a+d)*t+(2*a-5*b+4*d-e)*t*t+(-a+3*b-3*d+e)*t**3))
 return np.vstack([result,c[-1]])

def point(path,s):return np.array([np.interp(s,path['arc'],np.asarray(path['points'])[:,i]) for i in range(3)])
def frame(path,s):
 p=point(path,s);t=point(path,s+.2)-point(path,s-.2);t/=np.linalg.norm(t);side=np.array([-t[1],t[0],0]);side/=np.linalg.norm(side);return p,t,side

def landscape(main,centers,index):
 if not index:return []
 p=main['points'];a=main['half_width'];spec=[];count={1:3,2:5}[index]
 for ci,center in enumerate(centers):
  placed=[]
  for j,ds in enumerate(np.linspace(-5,5,count)):
   for sg in [-1,1]:
    q,t,side=frame(main,center);along=np.array([t[0],t[1],0.]);along/=np.linalg.norm(along);desired=q+along*ds*1.35+side*5.5*sg;candidates=[]
    for shift in np.linspace(-10,10,33):
     cp,ct,cs=frame(main,center+float(shift))
     for distance in [4.5,5.5,6.5,7.2]:
      candidate=cp+cs*sg*distance;nearby=abs(p[:,2]-candidate[2])<12;distances=np.linalg.norm(p[nearby,:2]-candidate[:2],axis=1)
      if distances.min()<3.5:continue
      closest=np.where(nearby)[0][int(np.argmin(distances))]
      if a[closest]<6.8 or distances.min()>a[closest]-1.2:continue
      if any(np.linalg.norm(candidate[:2]-previous[:2])<2.5 for previous in placed):continue
      candidates.append((float(np.linalg.norm(candidate-desired)),candidate))
    assert candidates,(index,ci,j,sg,'No clear landscape slot')
    q=min(candidates,key=lambda item:item[0])[1];placed.append(q)
    spec.append(dict(name=f'Landscape_{ci+1}_{j+1}_{sg}',center_m=q.tolist(),arc_m=float(center+ds),cavern_index=ci,radius_m=.72+.07*j,height_m=32. if index==1 else 2.6+.4*(j%3),tangent=[float(t[0]),float(t[1]),0],outward=(side*sg).tolist()))
 return spec

def plan(stage,index):
 if index==3:
  paths=json.loads((BASE/'RoutesV2'/stage/'centerlines.json').read_text());summary=json.loads((BASE/'RoutesV2'/stage/'plan_summary.json').read_text());summary.update(style=STYLES[index],geometry_retained=True)
  return paths,summary,json.loads((BASE/'RoutesV2'/stage/'neural_rocks.json').read_text())
 old=json.loads((BASE/stage/'centerlines.json').read_text())[0];op=np.array(old['points']);os=np.array(old['arc']);start=op[os<=9];end=op[os>=os[-1]-12].copy()
 if index==0:
  middle=spline([start[-1],[-47,-6,52],[-35,-9,46],[-23,3,40],[-7,8,33],[11,-4,26],[29,-8,19],[44,4,12],end[0]])
  p,s=sample(np.vstack([start,middle[1:],end[1:]]));center_targets=[[-35,-9,46],[29,-8,19]];spiral_turns=0;spiral_length=0
 elif index==1:
  end[:,2]+=END_RISE_M
  middle=spline([start[-1],[-49,-4,64],[-39,-13,70],[-24,-18,78],[-10,-6,85],[-20,10,91],[-4,23,98],[14,18,104],[24,0,109],[37,-10,110],end[0]])
  p,s=sample(np.vstack([start,middle[1:],end[1:]]));center_targets=[[-14,-10,83],[14,18,104]];spiral_turns=0;spiral_length=0
 else:
  cx=-3.;radius=23.;entry=np.array([cx-radius,0,53.]);lead=spline([start[-1],[-48,-7,57],[-38,-14,55],[-30,-8,54],entry])
  theta=np.linspace(math.pi,math.pi+math.tau*2.15,500);coil=np.column_stack([cx+radius*np.cos(theta),radius*np.sin(theta),np.linspace(53,11,len(theta))])
  tail=spline([coil[-1],[7,-31,10],[27,-25,9],[43,-9,8],end[0]])
  p,s=sample(np.vstack([start,lead[1:],coil[1:],tail[1:],end[1:]]));center_targets=[coil[int(len(coil)*.23)].tolist(),coil[int(len(coil)*.73)].tolist()];spiral_turns=2.15;spiral_length=float(np.linalg.norm(np.diff(coil,axis=0),axis=1).sum())
 u=s/s[-1];a=(1.65+.48*(.5+.5*np.sin(u*math.tau*3.7))) if index==0 else (2.9+.65*(.5+.5*np.sin(u*math.tau*3.1+index)))
 b=a*(.90 if index==0 else .95);centers=[float(s[np.argmin(np.linalg.norm(p-np.array(q),axis=1))]) for q in center_targets]
 for center in centers:
  bump=np.exp(-((s-center)/(6. if index==0 else 13.))**4);a=a*(1-bump)+[3.4,10,9.5][index]*bump;b=b*(1-bump)+[2.8,8,7.5][index]*bump
 for i,d in enumerate(s):
  if d<=9:a[i]=np.interp(d,os,old['half_width']);b[i]=np.interp(d,os,old['half_height'])
  elif d<15:
   w=(d-9)/6;a[i]=a[i]*w+np.interp(9,os,old['half_width'])*(1-w);b[i]=b[i]*w+np.interp(9,os,old['half_height'])*(1-w)
  remaining=s[-1]-d
  if remaining<=12:a[i]=np.interp(os[-1]-remaining,os,old['half_width']);b[i]=np.interp(os[-1]-remaining,os,old['half_height'])
  elif remaining<19:
   w=(remaining-12)/7;a[i]=a[i]*w+np.interp(os[-1]-12,os,old['half_width'])*(1-w);b[i]=b[i]*w+np.interp(os[-1]-12,os,old['half_height'])*(1-w)
 main=dict(name='Main',points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.35);paths=[main];gaps=[]
 for n,fraction in enumerate([.12,.36,.63,.86]):
  best=None;root_candidates=[]
  for offset in [0,-12,12,-24,24,-36,36,-48,48,-60,60,-80,80]:
   roots=sorted(s,key=lambda x:abs(x-s[-1]*fraction-offset));root=next(float(x) for x in roots if 20<x<s[-1]-25 and all(abs(x-c)>(8 if index==0 else 20) for c in centers))
   if root in root_candidates:continue
   root_candidates.append(root);q,t,side=frame(main,root)
   for sg in [-1,1]:
    for angle in [55,80,105,125,155]:
     direction=t.copy();direction[2]=0;direction/=np.linalg.norm(direction);d=direction*math.cos(math.radians(angle))+sg*side*math.sin(math.radians(angle))
     for length in [10,14,18,22]:
      climb=(1 if n%2==0 else -1)*(2 if index==0 else 4);curve=spline([q,q+d*length*.48+np.array([0,0,climb*.3]),q+d*length+direction*2+np.array([0,0,climb]),q+d*(length+5)+direction*5+np.array([0,0,climb+1])]);bp,bs=sample(curve);v=bs/bs[-1];aa=(1.45 if index==0 else 2.1)+(np.interp(root,s,a)*.75-(1.45 if index==0 else 2.1))*(1-v)**2;bb=aa*.95
      if np.max(abs(bp[:,1])+aa)>42 or np.min(bp[:,2]-bb)<2 or np.max(bp[:,2]+bb)>(125 if index==1 else 74):continue
      if np.min(bp[:,0]-aa)<-60 or np.max(bp[:,0]+aa)>60:continue
      mask=abs(s-root)>max(23,2*np.interp(root,s,a)+10);gap=float(np.min(np.linalg.norm(bp[:,None,:]-p[mask][None,:,:],axis=2)-np.maximum(aa,bb)[:,None]-np.maximum(a,b)[mask][None,:]))
      for oldpath in paths[1:]:gap=min(gap,float(np.min(np.linalg.norm(bp[:,None,:]-oldpath['points'][None,:,:],axis=2)-aa[:,None]-oldpath['radii'][None,:])))
      if best is None or gap>best[0]:best=(gap,dict(name=f'Branch_{n+1:02}',points=bp,arc=bs,half_width=aa,half_height=bb,radii=np.maximum(aa,bb)+.35,root_arc=root))
   if best and best[0]>1.5:break
  assert best and best[0]>.15,(stage,n,best[0] if best else None)
  paths.append(best[1]);gaps.append(best[0])
 dp=np.diff(p,axis=0);up=float(np.maximum(dp[:,2],0).sum());down=float(np.maximum(-dp[:,2],0).sum());total=sum(float(x['arc'][-1]) for x in paths)
 summary=dict(style=STYLES[index],main_length_m=float(s[-1]),total_length_m=total,main_drop_m=float(p[0,2]-p[-1,2]),start=p[0].tolist(),end=p[-1].tolist(),ascent_m=up,descent_m=down,ascending_vertical_fraction=up/(up+down),backtracking_x_m=float(np.maximum(-dp[:,0],0).sum()),spiral_turns=spiral_turns,spiral_drop_m=42 if index==2 else 0,spiral_length_m=spiral_length,spiral_main_fraction=spiral_length/s[-1],width_min_m=float(a.min()*2),width_max_m=float(a.max()*2),narrow_main_fraction=float(np.mean(a<2.5)),cavern_centers_arc_m=centers,branch_count=4,branch_clearances_m=gaps,landscape_count=12 if index==1 else 20 if index==2 else 0,mouth_collars_preserved_m=[9,12],end_shift_m=END_RISE_M if index==1 else 0)
 assert np.linalg.norm(p[0]-op[0])<1e-6;assert np.linalg.norm(p[-1]-op[-1]-np.array([0,0,summary['end_shift_m']]))<1e-6
 if index==0:assert summary['narrow_main_fraction']>.6
 if index==1:assert summary['ascending_vertical_fraction']>.8
 if index==2:assert summary['spiral_main_fraction']>.65
 return paths,summary,landscape(main,centers,index)
