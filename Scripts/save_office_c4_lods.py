"""Save and verify the reduced cinematic C4 without changing the player asset."""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/OfficeC4';prep=json.loads((out/'preparation.json').read_text(encoding='utf-8'))
m=unreal.load_asset(prep['asset']);assert m.get_num_lods()==2
triangles=[m.get_num_triangles(i) for i in range(m.get_num_lods())];assert 0<triangles[1]<850,triangles
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
for p,digest in prep['protected_hashes'].items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==digest,p
report=dict(passed=True,asset=prep['asset'],triangles=triangles,material_slots=len(m.static_materials),old_pool_cap=384,new_pool_cap=256,old_lod_triangles=598,new_max_visible_triangles=triangles[1]*256,old_max_visible_triangles=598*384,collision='None for instanced crowd; one sphere for collectible',pickup_scale=1.6)
(out/'assets.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log(json.dumps(report))
