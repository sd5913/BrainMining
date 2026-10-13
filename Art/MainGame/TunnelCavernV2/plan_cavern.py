"""Second descending route, two wider caverns, real rock-column voids, metres."""
import numpy as np, math, json, runpy
from pathlib import Path
OUT=Path(__file__).parent
helpers=runpy.run_path(str(OUT.parents[1]/'Mining/TunnelWideV2/plan_wide.py'),run_name='helpers')
old=json.loads((OUT.parent/'TunnelV1/centerlines.json').read_text())[0]

def sample(dense,step=.35):
    arc=np.r_[0,np.cumsum(np.linalg.norm(np.diff(dense,axis=0),axis=1))]
    s=np.linspace(0,arc[-1],math.ceil(arc[-1]/step)+1)
    return np.column_stack([np.interp(s,arc,dense[:,i]) for i in range(3)]),s

def plan():
    xy=np.array([[-65,0],[-50,2],[-36,9],[-20,5],[-7,-11],[9,-13],[7,5],[25,12],[38,7],[52,-2],[65,0]],float)
    pad=np.vstack([2*xy[0]-xy[1],xy,2*xy[-1]-xy[-2]])
    dense=[]
    for k in range(len(xy)-1):
        a,b,c,d=pad[k:k+4]
        for t in np.linspace(0,1,100,endpoint=False):
            dense.append(.5*(2*b+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t**3))
    dense=np.vstack([dense,xy[-1]])
    h=np.r_[0,np.cumsum(np.linalg.norm(np.diff(dense,axis=0),axis=1))]
    p,s=sample(np.column_stack([dense,65-55*h/h[-1]]));u=s/s[-1]
    a=1.65+3.25*(.5+.5*np.sin(2*math.pi*3.05*u+1.15))**1.15
    b=a*(.97+.11*np.sin(2*math.pi*1.9*u+1.))
    # Raised broad windows, while the rest remains comparable to the previous route.
    cavern_centers=[float(s[-1]*.43),float(s[-1]*.70)]
    for center in cavern_centers:
        bump=np.exp(-((s-center)/8.0)**4)
        a=a*(1-bump)+6.7*bump;b=b*(1-bump)+5.2*bump
    main=dict(name='Main',points=p,arc=s,half_width=a,half_height=b,radii=np.maximum(a,b)+.35)
    paths=[main];clearances=[]
    for n,fraction in enumerate([.17,.32,.58,.80]):
        i=int(np.argmin(abs(s-s[-1]*fraction)));tangent=p[min(i+2,len(p)-1)]-p[max(i-2,0)];candidates=[]
        for length in [16.,17.,18.]:
            for sign in [-1,1]:
                for angle in [65,80,95,110,125]:
                    for bend in [-3.,0,3.]:
                        branch=helpers['branch'](p[i],tangent,length,math.radians(sign*angle),bend,6.5,n*.93)
                        if branch['arc'][-1]>20:continue
                        v=branch['arc']/branch['arc'][-1]
                        aa=np.maximum(1.65,main['half_width'][i]*.78*(1-v)+1.65*v);bb=aa*(.96+.07*np.sin(2*math.pi*v+n+.5))
                        branch.update(half_width=aa,half_height=bb,radii=np.maximum(aa,bb)+.35)
                        if np.min(branch['points'][:,0]-branch['radii'])<-60 or np.max(branch['points'][:,0]+branch['radii'])>60:continue
                        if np.min(branch['points'][:,2]-branch['radii'])<11:continue
                        gap=helpers['gap'](branch,helpers['subset'](main,abs(s-s[i])>15))
                        gap=min(gap,helpers['gap'](helpers['subset'](branch,branch['arc']>11),main))
                        for other in paths[1:]:gap=min(gap,helpers['gap'](branch,other))
                        candidates.append((gap,branch))
        assert candidates,n
        gap,branch=max(candidates,key=lambda q:q[0]);assert gap>.55,(n,gap)
        branch.update(name=f'Branch_{n+1:02}',root_arc=float(s[i]));paths.append(branch);clearances.append(gap)
    for path in paths:assert np.all(np.diff(path['points'][:,2])<0),path['name']
    pp=p[::2];rr=main['radii'][::2];ss=s[::2];mask=abs(ss[:,None]-ss[None,:])>32
    separation=np.linalg.norm(pp[:,None,:]-pp[None,:,:],axis=2)-rr[:,None]-rr[None,:]
    assert separation[mask].min()>.5,float(separation[mask].min())
    pillars=[]
    for ci,center in enumerate(cavern_centers):
        for j,(ds,offset) in enumerate([(-2.,-1.9),(0.,1.3),(2.,-1.1)]):
            arc=center+ds;point=np.array([np.interp(arc,s,p[:,i]) for i in range(3)])
            tangent=np.array([np.interp(arc,s,np.gradient(p,axis=0)[:,i]) for i in range(3)])
            side=np.array([-tangent[1],tangent[0],0.]);side/=np.linalg.norm(side)
            point+=side*offset
            pillars.append(dict(name=f'Pillar_{ci+1}_{j+1}',center_m=point.tolist(),radius_m=.7+.12*j,height_m=18.,arc_m=arc,cavern_index=ci))
    serial=[{k:v.tolist() if isinstance(v,np.ndarray) else v for k,v in path.items()} for path in paths]
    (OUT/'centerlines.json').write_text(json.dumps(serial,indent=2))
    (OUT/'pillars.json').write_text(json.dumps(pillars,indent=2))
    report=dict(main_length_m=float(s[-1]),main_drop_m=55.,start=p[0].tolist(),end=p[-1].tolist(),
        width_min_m=float(2*a.min()),width_max_m=float(2*a.max()),branch_count=4,branch_lengths_m=[float(q['arc'][-1]) for q in paths[1:]],
        branch_clearances_m=clearances,cavern_centers_arc_m=cavern_centers,cavern_width_m=13.4,cavern_height_m=10.4,pillar_count=6,
        reserved_connection_length_m=18.,reserved_connection_width_m=12.,reserved_connection_height_m=8.)
    (OUT/'plan_summary.json').write_text(json.dumps(report,indent=2));print('CAVERN_PLAN '+json.dumps(report),flush=True)
    return paths
if __name__=='__main__':plan()
