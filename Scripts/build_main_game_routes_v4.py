"""Facet the existing regional paths, retaining their gameplay and landscape plan."""
import sys,json,copy
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Scripts'))
code=(ROOT/'Scripts/build_main_game_routes_v3.py').read_text(encoding='utf-8').split('wanted=next(')[0]
code=code.replace("source=source[:source.index(\"if __name__=='__main__':\")]", "source=source.replace(\"OUTROOT=BASE/'RoutesV3'\",\"OUTROOT=BASE/'RoutesV4'\")\nsource=source.replace('smooth.factor=.65;smooth.iterations=4','smooth.factor=.25;smooth.iterations=1')\nsource=source.replace('17000/len(tunnel.data.polygons)','facet_budget/len(tunnel.data.polygons)')\nsource=source[:source.index(\"if __name__=='__main__':\")]")
ns={'__file__':str(ROOT/'Scripts/build_main_game_routes_v3.py'),'__name__':'faceted_route_helpers'}
exec(compile(code,'faceted_route_helpers','exec'),ns)
scope=ns['scope'];np=scope['np']
# Keep the exact authored route and scenery positions, independently of planner revisions.
def current_plan(stage,index):
 folder=ROOT/'Art/MainGame/RoutesV3'/stage
 paths=json.loads((folder/'centerlines.json').read_text())
 for p in paths:
  for key in ['points','arc','half_width','half_height','radii']:p[key]=np.array(p[key])
 summary=json.loads((folder/'plan_summary.json').read_text());summary.update(surface_style='angular-lowpoly',geometry_retained=False)
 name={1:'pillars.json',2:'stalagmites.json',3:'neural_rocks.json'}.get(index)
 return paths,summary,json.loads((folder/name).read_text()) if name else []
scope['plan']=current_plan
original_tube=scope['wide']['tube']
tube_code=(ROOT/'Art/Mining/TunnelWideV2/build_wide.py').read_text().split('def materials(')[0].replace('sides=40','sides=10')
tube_ns=dict(scope);exec(compile(tube_code,'angular_tube','exec'),tube_ns)
def faceted_tube(path):
 if 'arc' not in path:return original_tube(path) # Small neural filaments retain their authored detail.
 p=copy.deepcopy(path);arc=np.array(p['arc']);s=np.unique(np.r_[np.arange(0,arc[-1],1.5),arc[-1]])
 for key in ['points','half_width','half_height']:
  data=np.array(p[key]);p[key]=np.column_stack([np.interp(s,arc,data[:,j]) for j in range(3)]) if data.ndim==2 else np.interp(s,arc,data)
 p['arc']=s
 return tube_ns['tube'](p)
scope['wide']['tube']=faceted_tube
wanted=next((a.split('=',1)[1] for a in sys.argv if a.startswith('--stage=')),None)
for index,stage in enumerate(ns['planner'].STAGES):
 if wanted and wanted!=stage:continue
 if index==3:
  import runpy
  runpy.run_path(str(ROOT/'Scripts/facet_main_game_neural_v4.py'),run_name='__main__')
 else:
  scope['facet_budget']=[1800,3000,4800][index]
  scope['build'](stage,index)
  if index==1:
   import runpy
   runpy.run_path(str(ROOT/'Scripts/facet_main_game_cavern_v4.py'),run_name='__main__')
