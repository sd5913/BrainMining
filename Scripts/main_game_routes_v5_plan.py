"""Three distinct routes in metres; original office and journey mouths retained."""
import json,math
from pathlib import Path
import numpy as np
from main_game_routes_v3_plan import sample,spline,point,frame,plan as old_plan
ROOT=Path(__file__).resolve().parents[1]
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']

def path(name,controls,width=2,height=None):
 p,s=sample(spline(controls));u=s/s[-1]
 a=width*(1+.12*np.sin(s*.38)+.07*np.sin(s*.91));b=(height or width)* (1+.1*np.sin(s*.29+1))
 return dict(name=name,points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.45)

def smooth_branches(paths):
 main=paths[0]
 for i,p in enumerate(paths[1:]):
  q,t,side=frame(main,p['root_arc']);original=np.array(p['points']);length=p['arc'][-1]
  # Tangential forks have the same cross-section as the main passage at entry.
  controls=[q,q+t*3,original[len(original)//3],original[2*len(original)//3],original[-1]]
  bp,bs=sample(spline(controls));v=bs/bs[-1];w=np.interp(p['root_arc'],main['arc'],main['half_width'])
  p.update(points=bp,arc=bs,half_width=1.6+(w-1.6)*(1-v)**2,half_height=1.65+(w-1.65)*(1-v)**2)
  p['radii']=np.maximum(p['half_width'],p['half_height'])+.45

def plan(stage,index):
 features=dict(platforms=[],spires=[],neural=[],pillars=[],upward_gates=[])
 if index==0:
  paths,summary,_=old_plan(stage,index);smooth_branches(paths)
  main=paths[0]
  for n,s in enumerate(np.linspace(25,main['arc'][-1]-22,16)):
   q,t,side=frame(main,s);width=np.interp(s,main['arc'],main['half_width']);q+=side*((-1)**n)*width*.68
   features['spires'].append(dict(center_m=q.tolist(),ceiling=bool(n%2),height_m=1.15+.22*(n%4),radius_m=.45+.08*(n%3)))
 else:
  old=json.loads((ROOT/'Art/MainGame/RoutesV4'/stage/'centerlines.json').read_text())[0]
  start=np.array(old['points'])[:24];end=np.array(old['points'])[-32:]
  if index==1:
   controls=[start[-1],[-49,-4,65],[-40,-5,68],[-34,0,70],[-27,0,76],[-18,0,82],[-18,0,91],[-10,8,92],[0,5,94],[8,0,96],[16,0,100],[24,0,104],[24,0,114],[34,-7,113],[45,-4,111],end[0]]
   p,s=sample(np.vstack([start,spline(controls)[1:],end[1:]]));main=path('Main',[[0,0,0],[1,0,0]])
   main.update(points=p,arc=s,half_width=2.+.25*np.sin(s*.3),half_height=2.1+.18*np.sin(s*.25))
   centers=[]
   for ci,(target,exitp) in enumerate([([-28,0,76],[-18,0,87]),([14,0,100],[24,0,109])]):
    c=float(s[np.argmin(np.linalg.norm(p-target,axis=1))]);centers.append(c)
    bump=np.exp(-((s-c)/10.0)**6);main['half_width']=main['half_width']*(1-bump)+10*bump;main['half_height']=main['half_height']*(1-bump)+8*bump
    features['upward_gates'].append(exitp)
    # Columns reach solid floor, with staggered broad flat landing surfaces.
    for n in range(5):
     x=target[0]-6+n*3.5;y=(-1 if n%2 else 1)*2.3;z=target[2]-5+n*2.4
     if n==4:x-=1;y=0;z=target[2]+3
     features['platforms'].append(dict(name=f'Parkour_{ci+1}_{n+1}',top_m=[x,y,z],base_z_m=target[2]-17,radius_m=1.45,chamber=ci))
   roots=[18,80,140,155]
   summary=dict(style='ascending-jetpack-chambers',cavern_centers_arc_m=centers,parkour_chambers=2,spiral_turns=0)
  else:
   segments=[start];prev=start[-1];coil_records=[];centers=[]
   for n,(cx,cy,z) in enumerate([(-37,-2,55),(-4,4,37),(29,-3,19)]):
    theta=np.linspace(math.pi,math.pi+math.tau*1.1,140);coil=np.column_stack([cx+5.5*np.cos(theta),cy+5.5*np.sin(theta),np.linspace(z,z-11,len(theta))])
    controls=[prev,(prev+coil[0])*.5,coil[0]]
    if n: controls=[prev,[cx-19,0,z+7],[cx-12,-3,z+3],coil[0]];centers.append([cx-19,0,z+7])
    segments += [spline(controls)[1:],coil[1:]];prev=coil[-1];coil_records.append(dict(center=[cx,cy],radius_m=5.5,turns=1.1,drop_m=11))
   segments.append(spline([prev,[39,-10,8],[52,-6,12],[64,0,15],[74.5,0,15],[74.5,0,7]])[1:])
   p,s=sample(np.vstack(segments));main=dict(name='Main',points=p,arc=s,half_width=1.85+.20*np.sin(s*.4),half_height=1.9+.18*np.sin(s*.3))
   arcs=[]
   for ci,target in enumerate(centers):
    c=float(s[np.argmin(np.linalg.norm(p-target,axis=1))]);arcs.append(c);bump=np.exp(-((s-c)/7.)**4)
    main['half_width']=main['half_width']*(1-bump)+8*bump;main['half_height']=main['half_height']*(1-bump)+6*bump
    for n in range(4 if ci==0 else 2):
     q,t,side=frame(main,c+((n//2-.5)*4 if ci==0 else 0));q+=side*((-1)**n)*(4.8 if ci==0 else 3.8)
     spec=dict(name=f'Scenery_{ci}_{n}',center_m=q.tolist(),arc_m=float(c),tangent=[float(t[0]),float(t[1]),0],outward=(side*((-1)**n)).tolist(),radius_m=.75,height_m=32)
     features['pillars' if ci==0 else 'neural'].append(spec)
   roots=[20,s[-1]*.37,s[-1]*.65,s[-1]-.12*s[-1]]
   summary=dict(style='tight-descending-coils',cavern_centers_arc_m=arcs,coils=coil_records,spiral_turns=3.3,museum_drop_m=[74.5,0,7])
  # Exact original entry collar. Region 2 also keeps its established exit.
  for j,d in enumerate(s):
   if d<=8:
    main['half_width'][j]=np.interp(d,old['arc'],old['half_width']);main['half_height'][j]=np.interp(d,old['arc'],old['half_height'])
   if index==1 and s[-1]-d<11:
    old_s=old['arc'][-1]-(s[-1]-d);main['half_width'][j]=np.interp(old_s,old['arc'],old['half_width']);main['half_height'][j]=np.interp(old_s,old['arc'],old['half_height'])
  main['radii']=np.maximum(main['half_width'],main['half_height'])+.45;paths=[main]
  for n,preferred in enumerate(roots):
   best=None
   for offset in [0,-5,5,-10,10,-20,20,-30,30]:
    root=float(np.clip(preferred+offset,16,s[-1]-20));q,t,side=frame(main,root)
    if np.interp(root,s,main['half_width'])>3:continue
    for sg in [-1,1]:
     for forward in [-8,0,8]:
      for length in [15,20,25]:
       branch=path(f'Branch_{n+1:02}',[q,q+t*3,q+t*forward*.5+side*sg*length*.5,q+t*forward+side*sg*length+np.array([0,0,(-1)**n*3])],1.7)
       bp=branch['points'];mask=abs(s-root)>(12 if index==2 else 20);gap=float(np.min(np.linalg.norm(bp[::3,None,:]-p[None,mask,:],axis=2)-branch['radii'][::3,None]-main['radii'][None,mask]))
       for prevpath in paths[1:]:
        bi=np.where(branch['arc']>7)[0][::3];pi=np.where(prevpath['arc']>7)[0][::3]
        gap=min(gap,float(np.min(np.linalg.norm(bp[bi,None,:]-prevpath['points'][None,pi,:],axis=2)-branch['radii'][bi,None]-prevpath['radii'][None,pi])))
       if bp[:,0].min()<-60 or bp[:,0].max()>70:continue
       if best is None or gap>best[0]:best=(gap,branch,root)
   assert best and best[0]>.1,(stage,n,best[0] if best else None)
   branch=best[1];branch['root_arc']=best[2];paths.append(branch)
 main=paths[0];dp=np.diff(main['points'],axis=0)
 summary.update(start=main['points'][0].tolist(),end=main['points'][-1].tolist(),main_length_m=float(main['arc'][-1]),total_length_m=sum(float(p['arc'][-1]) for p in paths),branch_count=4,ascent_m=float(np.maximum(dp[:,2],0).sum()),descent_m=float(np.maximum(-dp[:,2],0).sum()),width_min_m=float(main['half_width'].min()*2),width_max_m=float(main['half_width'].max()*2),surface_style='irregular-asymmetric-lowpoly')
 return paths,summary,features
