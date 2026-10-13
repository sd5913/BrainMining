"""Compare centreline shape at equal route progress, excluding world-placement translation."""
import json,numpy as np
from pathlib import Path
OUT=Path(__file__).parent
new=json.loads((OUT/'centerlines.json').read_text())[0]
old=json.loads((OUT.parent/'TunnelStalagmiteV3/centerlines.json').read_text())[0]
u=np.linspace(0,1,1001)
def positions(path):
    s=np.array(path['arc']);p=np.array(path['points'])
    return np.column_stack([np.interp(u,s/s[-1],p[:,i]) for i in range(3)])
a=positions(new);b=positions(old);mirror=b*np.array([1,-1,1])
delta=np.linalg.norm(a-b,axis=1);mirror_delta=np.linalg.norm(a-mirror,axis=1)
assert delta.max()>10 and mirror_delta.max()>8,(delta.max(),mirror_delta.max())
report=dict(passed=True,progress_samples=len(u),max_route_shape_difference_m=float(delta.max()),
    mean_route_shape_difference_m=float(delta.mean()),max_difference_from_y_mirror_m=float(mirror_delta.max()),
    new_length_m=float(new['arc'][-1]),previous_length_m=float(old['arc'][-1]),world_translation_excluded=True)
(OUT/'path_change_validation.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)
