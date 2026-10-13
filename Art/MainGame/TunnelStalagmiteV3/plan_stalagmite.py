"""A fresh mirrored/reshaped descending route and two clusters of ground stalagmites."""
from pathlib import Path
import json
OUT=Path(__file__).parent
code=(OUT.parent/'TunnelCavernV2/plan_cavern.py').read_text()
code=code.replace("old=json.loads((OUT.parent/'TunnelV1/centerlines.json').read_text())[0]", "old=json.loads((OUT.parent/'TunnelCavernV2/centerlines.json').read_text())[0]")
code=code.replace('[[-65,0],[-50,2],[-36,9],[-20,5],[-7,-11],[9,-13],[7,5],[25,12],[38,7],[52,-2],[65,0]]',
                  '[[-65,0],[-50,-2],[-36,-9],[-20,-6],[-7,10],[10,12],[7,-6],[26,-11],[40,-5],[53,2],[65,0]]')
code=code.replace('3.05*u+1.15','3.15*u+.8').replace('6.7*bump','6.1*bump').replace('5.2*bump','4.9*bump')
code=code.replace('[.17,.32,.58,.80]','[.18,.34,.58,.80]')
start=code.index('    pillars=[]');end=code.index('    serial=',start)
code=code[:start]+'''    stalagmites=[]
    for ci,center in enumerate(cavern_centers):
        for j in range(6):
            ds=[-2.,0.,2.][j//2];offset=(-1 if j%2==0 else 1)*(2.15+.2*(j//2))
            arc=center+ds;point=np.array([np.interp(arc,s,p[:,i]) for i in range(3)])
            tangent=np.array([np.interp(arc,s,np.gradient(p,axis=0)[:,i]) for i in range(3)])
            side=np.array([-tangent[1],tangent[0],0.]);side/=np.linalg.norm(side);point+=side*offset
            stalagmites.append(dict(name=f'Stalagmite_{ci+1}_{j+1}',center_m=point.tolist(),radius_m=.52+.09*((j+ci)%4),
                height_m=1.2+.32*((j+2*ci)%6),arc_m=arc,cavern_index=ci,centerline_clearance_m=abs(offset)-(.52+.09*((j+ci)%4))))
'''+code[end:]
code=code.replace("(OUT/'pillars.json').write_text(json.dumps(pillars,indent=2))","(OUT/'stalagmites.json').write_text(json.dumps(stalagmites,indent=2))")
code=code.replace('cavern_width_m=13.4,cavern_height_m=10.4,pillar_count=6','cavern_width_m=12.2,cavern_height_m=9.8,stalagmite_count=12')
exec(compile(code,str(OUT/'plan_stalagmite_generated.py'),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
