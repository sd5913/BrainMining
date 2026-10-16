"""Read current player C4 and office cinematic geometry budgets."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/OfficeC4';out.mkdir(parents=True,exist_ok=True)
report={}
for path in ['/Game/Bomb/SM_StickyBomb','/Game/Bomb/SM_Bomb','/Game/PopupClose/FX/SM_OfficeFinaleBomb']:
    m=unreal.load_asset(path);bounds=m.get_bounds()
    report[path]=dict(triangles=[m.get_num_triangles(i) for i in range(m.get_num_lods())],origin=list(bounds.origin.to_tuple()),extent=list(bounds.box_extent.to_tuple()),materials=[s.material_interface.get_path_name() if s.material_interface else None for s in m.static_materials])
(out/'before.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log(json.dumps(report))
