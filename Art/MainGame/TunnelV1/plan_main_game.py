"""A fresh compact descending tree, in metres; no FullJourney room geometry."""
import numpy as np,math,json,runpy
from pathlib import Path
from mathutils import Vector
OUT=Path(__file__).parent
helpers=runpy.run_path(str(OUT.parents[1]/'Mining/TunnelWideV2/plan_wide.py'),run_name='helpers')

def sample(dense,step=.35):
    arc=np.r_[0,np.cumsum(np.linalg.norm(np.diff(dense,axis=0),axis=1))]
    s=np.linspace(0,arc[-1],math.ceil(arc[-1]/step)+1)
    p=np.column_stack([np.interp(s,arc,dense[:,i]) for i in range(3)])
    return p,s

def make_main():
    xy=np.array([[-65,0],[-50,0],[-35,7],[-22,10],[-9,-6],[6,-9],[3,6],[21,9],[37,4],[50,0],[65,0]],float)
    pad=np.vstack([2*xy[0]-xy[1],xy,2*xy[-1]-xy[-2]])
    dense=[]
    for k in range(len(xy)-1):
        a,b,c,d=pad[k:k+4]
        for t in np.linspace(0,1,100,endpoint=False):
            dense.append(.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t**3))
    dense=np.vstack([dense,xy[-1]])
    h=np.r_[0,np.cumsum(np.linalg.norm(np.diff(dense,axis=0),axis=1))]
    dense=np.column_stack([dense,65-55*h/h[-1]])
    p,s=sample(dense);u=s/s[-1]
    wave=(.5+.5*np.sin(2*math.pi*3.10*u+.4))**1.1
    a=1.65+3.25*wave;aspect=.98+.09*np.sin(2*math.pi*1.75*u+.7);b=a*aspect
    return dict(name='Main',points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.35)

def plan():
    main=make_main();paths=[main];clearances=[]
    for n,fraction in enumerate([.18,.38,.60,.80]):
        s=main['arc'];p=main['points'];i=int(np.argmin(abs(s-s[-1]*fraction)))
        tangent=p[min(i+2,len(p)-1)]-p[max(i-2,0)];candidates=[]
        for length in [15.,16.,17.]:
            for sign in [-1,1]:
                for angle in [65,80,95,110,125]:
                    for bend in [-3.,0,3.]:
                        path=helpers['branch'](p[i],tangent,length,math.radians(sign*angle),bend,6.5,n*.83)
                        if path['arc'][-1]>20:continue
                        u=path['arc']/path['arc'][-1]
                        a=(main['half_width'][i]*.8)*(1-u)+1.65*u
                        a=np.maximum(a,1.65);b=a*(.98+.06*np.sin(2*math.pi*u+n))
                        path.update(half_width=a,half_height=b,radii=np.maximum(a,b)+.35)
                        if np.min(path['points'][:,0]-path['radii'])<-60 or np.max(path['points'][:,0]+path['radii'])>60:continue
                        if np.min(path['points'][:,2]-path['radii'])<11:continue
                        gap=helpers['gap'](path,helpers['subset'](main,abs(s-s[i])>14))
                        gap=min(gap,helpers['gap'](helpers['subset'](path,path['arc']>11),main))
                        for old in paths[1:]:gap=min(gap,helpers['gap'](path,old))
                        candidates.append((gap,path))
        assert candidates,('no branch candidates',n,float(p[i,2]))
        gap,path=max(candidates,key=lambda q:q[0]);assert gap>.55,(n,gap)
        path.update(name=f'Branch_{n+1:02}',root_arc=float(s[i]));paths.append(path);clearances.append(gap)
    for path in paths:assert np.all(np.diff(path['points'][:,2])<0),path['name']
    points=main['points'][::2];r=main['radii'][::2];s=main['arc'][::2]
    mask=abs(s[:,None]-s[None,:])>28
    separation=np.linalg.norm(points[:,None,:]-points[None,:,:],axis=2)-r[:,None]-r[None,:]
    assert separation[mask].min()>.6,float(separation[mask].min())
    OUT.mkdir(parents=True,exist_ok=True)
    serial=[{k:v.tolist() if isinstance(v,np.ndarray) else v for k,v in path.items()} for path in paths]
    (OUT/'centerlines.json').write_text(json.dumps(serial,indent=2))
    result=dict(main_length_m=float(main['arc'][-1]),main_drop_m=55.,start=main['points'][0].tolist(),end=main['points'][-1].tolist(),
                width_min_m=float(2*main['half_width'].min()),width_max_m=float(2*main['half_width'].max()),branch_count=4,
                branch_lengths_m=[float(p['arc'][-1]) for p in paths[1:]],branch_clearances_m=clearances)
    (OUT/'plan_summary.json').write_text(json.dumps(result,indent=2));print('MAIN_GAME_PLAN '+json.dumps(result),flush=True)
    return paths
if __name__=='__main__':plan()
