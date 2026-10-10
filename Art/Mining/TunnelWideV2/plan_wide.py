"""Plan the widened descending tree before making any Blender geometry (metres)."""
import json, math
from pathlib import Path
import numpy as np

OUTPUT=Path(__file__).parent

def resample(dense, step=.65):
    arc=np.r_[0,np.cumsum(np.linalg.norm(np.diff(dense,axis=0),axis=1))]
    s=np.linspace(0,arc[-1],math.ceil(arc[-1]/step)+1)
    p=np.column_stack([np.interp(s,arc,dense[:,j]) for j in range(3)])
    return p,s

def sections(s, phase=0, branch=False):
    u=s/s[-1]
    # Old nominal diameter is about 5.1 m. Typical new width is about twice this.
    wave=(.5+.5*np.sin(2*np.pi*(3.8 if not branch else 1.25)*u+phase))**1.6
    a=2.70*(1+2.55*wave)
    aspect=.98+.18*np.sin(2*np.pi*1.8*u+phase+.8)
    b=a*aspect
    if branch:
        # Rounded tips retain a full tube until the final cap.
        a*=1-.14*u**5
        b*=1-.14*u**5
    return a,b

def main_path():
    xy=np.array([[-135,-28],[-60,-8],[-47,5],[-30,22],[-40,40],[-25,57],
                 [0,49],[16,30],[5,8],[22,-13],[49,-4],[38,18],[63,35],[135,55]],float)
    xy[1:-1]*=1.45
    pad=np.vstack([2*xy[0]-xy[1],xy,2*xy[-1]-xy[-2]])
    dense=[]
    for k in range(len(xy)-1):
        a,b,c,d=pad[k:k+4]
        for t in np.linspace(0,1,100,endpoint=False):
            q=.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t**3)
            dense.append([*q,65-174*(k+t)/(len(xy)-1)])
    dense.append([*xy[-1],-109])
    p,s=resample(np.array(dense));a,b=sections(s,.15)
    return dict(name='Main',points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.65)

def branch(root,tangent,length,angle,bend,drop,phase):
    f=tangent[:2]/np.linalg.norm(tangent[:2]);side=np.array([-f[1],f[0]])
    d=math.cos(angle)*f+math.sin(angle)*side;cross=np.array([-d[1],d[0]])
    controls=np.array([root,root+np.r_[d*length*.30,-drop*.26],
                       root+np.r_[d*length*.73+cross*bend,-drop*.67],
                       root+np.r_[d*length+cross*bend*.5,-drop]])
    t=np.linspace(0,1,260)[:,None]
    dense=(1-t)**3*controls[0]+3*(1-t)**2*t*controls[1]+3*(1-t)*t*t*controls[2]+t**3*controls[3]
    p,s=resample(dense);a,b=sections(s,phase,True)
    return dict(points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.65,controls=controls)

def gap(a,b):
    return float(np.min(np.linalg.norm(a['points'][::3,None,:]-b['points'][None,::3,:],axis=2)-a['radii'][::3,None]-b['radii'][None,::3]))

def subset(path,mask):
    return {k:v[mask] for k,v in path.items() if k in ('points','radii')}

def plan():
    main=main_path();paths=[main];clearances=[]
    for n,height in enumerate([40,22,4,-14,-32,-50,-68]):
        s=main['arc'];p=main['points'];i=int(np.argmin(abs(p[:,2]-height)))
        tangent=p[min(i+2,len(p)-1)]-p[max(i-2,0)]
        candidates=[]
        for length in [44,54,64]:
            for sign in [-1,1]:
                for deg in [65,80,95,110,125,145]:
                    for bend in [-15,0,15]:
                        drop=min(24,float(p[i,2]+99))
                        if drop<10:continue
                        bp=branch(p[i],tangent,length,math.radians(sign*deg),bend,drop,n*.93)
                        # Endpoints of the main are also the extrema in the front view.
                        if np.min(bp['points'][:,0]-bp['radii'])<-129 or np.max(bp['points'][:,0]+bp['radii'])>129:continue
                        if np.min(bp['points'][:,2]-bp['radii'])<-105:continue
                        g=gap(bp,subset(main,abs(s-s[i])>32))
                        g=min(g,gap(subset(bp,bp['arc']>28),main))
                        for old in paths[1:]:g=min(g,gap(bp,old))
                        candidates.append((g,bp))
        if not candidates:raise RuntimeError(('no candidates',n))
        g,bp=max(candidates,key=lambda v:v[0])
        if g<1.2:raise RuntimeError(('branch clearance',n,g))
        bp.update(name=f'Branch_{n+1:02}',root_arc=float(s[i]));paths.append(bp);clearances.append(g)
    for path in paths:
        assert np.all(np.diff(path['points'][:,2])<0)
        p=path['points'][::3];r=path['radii'][::3];s=path['arc'][::3]
        mask=abs(s[:,None]-s[None,:])>45
        if mask.any():
            d=np.linalg.norm(p[:,None,:]-p[None,:,:],axis=2)-r[:,None]-r[None,:]
            assert float(d[mask].min())>1.2,(path['name'],d[mask].min())
    serial=[{k:v.tolist() if isinstance(v,np.ndarray) else v for k,v in path.items()} for path in paths]
    OUTPUT.mkdir(parents=True,exist_ok=True)
    (OUTPUT/'centerlines.json').write_text(json.dumps(serial,indent=2),encoding='utf-8')
    report={'main_length_m':float(main['arc'][-1]),'start':main['points'][0].tolist(),'end':main['points'][-1].tolist(),
            'branch_clearances_m':clearances,'width_min_m':float(2*main['half_width'].min()),
            'width_max_m':float(2*main['half_width'].max()),'width_mean_m':float(2*main['half_width'].mean()),'branch_count':7}
    (OUTPUT/'plan_summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report),flush=True)
    return paths

if __name__=='__main__':plan()
