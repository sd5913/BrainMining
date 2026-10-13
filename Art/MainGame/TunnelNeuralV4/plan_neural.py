"""A new three-sweep S route, not a mirror or copy of the previous centreline."""
from pathlib import Path
OUT=Path(__file__).parent
code=(OUT.parent/'TunnelCavernV2/plan_cavern.py').read_text()
code=code.replace('[[-65,0],[-50,2],[-36,9],[-20,5],[-7,-11],[9,-13],[7,5],[25,12],[38,7],[52,-2],[65,0]]',
                  '[[-65,0],[-52,1],[-42,9],[-27,12],[-14,2],[-2,-12],[14,-8],[24,8],[40,10],[52,3],[65,0]]')
code=code.replace('3.05*u+1.15','2.85*u+.65').replace('6.7*bump','6.4*bump').replace('5.2*bump','5.0*bump')
code=code.replace('[.17,.32,.58,.80]','[.17,.32,.58,.80]')
start=code.index('    pillars=[]');end=code.index('    serial=',start)
code=code[:start]+'''    neural_rocks=[]
    for ci,center in enumerate(cavern_centers):
        for j,sign in enumerate([-1,1]):
            arc=center;point=np.array([np.interp(arc,s,p[:,i]) for i in range(3)])
            tangent=np.array([np.interp(arc,s,np.gradient(p,axis=0)[:,i]) for i in range(3)]);tangent[2]=0;tangent/=np.linalg.norm(tangent)
            outward=sign*np.array([-tangent[1],tangent[0],0.]);point+=outward*3.3
            neural_rocks.append(dict(name=f'NeuralRock_{ci+1}_{j+1}',center_m=point.tolist(),arc_m=arc,cavern_index=ci,
                tangent=list(tangent),outward=list(outward),height_m=4.4,primary_branch_count=6,secondary_branch_count=12))
'''+code[end:]
code=code.replace("(OUT/'pillars.json').write_text(json.dumps(pillars,indent=2))","(OUT/'neural_rocks.json').write_text(json.dumps(neural_rocks,indent=2))")
code=code.replace('cavern_width_m=13.4,cavern_height_m=10.4,pillar_count=6','cavern_width_m=12.8,cavern_height_m=10.,neural_rock_count=4')
exec(compile(code,str(OUT/'plan_neural_generated.py'),'exec'),{'__file__':str(__file__),'__name__':'__main__'})
