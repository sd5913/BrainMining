"""Validate final offline meshes and unchanged route centerlines."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];result=dict(passed=True,regions=[])
for stage in ['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3','TunnelNeuralV4']:
 old=ROOT/'Art/MainGame/RoutesV3'/stage;new=ROOT/'Art/MainGame/RoutesV4'/stage
 before=json.loads((old/'tunnel_validation.json').read_text());after=json.loads((new/'tunnel_validation.json').read_text())
 assert json.loads((old/'centerlines.json').read_text())==json.loads((new/'centerlines.json').read_text()),stage
 assert after['passed'] and after['connected_components']==1 and after['signed_volume_m3']>0
 assert all(after[k]==0 for k in ['boundary_edges','nonmanifold_edges','inconsistent_winding_edges','degenerate_faces','duplicate_faces','self_intersection_pairs'])
 assert len(json.loads((new/'placement_plan.json').read_text())['objects'])==8
 result['regions'].append(dict(stage=stage,before_faces=before['faces'],after_faces=after['faces'],face_reduction=1-after['faces']/before['faces'],centerlines_unchanged=True,closed=True,no_self_intersections=True))
(ROOT/'Saved/Validation/MainGameRoutesV4/geometry.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result))
