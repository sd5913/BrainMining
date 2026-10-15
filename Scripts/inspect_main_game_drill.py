"""Inspect authored first-person drill components without modifying assets."""
import unreal, json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/MainGameDrill';out.mkdir(parents=True,exist_ok=True)
report={}
for path in ['/Game/MainGame/Blueprints/BP_MainGameCharacter','/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter']:
    bp=unreal.load_asset(path);cdo=unreal.get_default_object(bp.generated_class());items=[]
    for c in cdo.get_components_by_class(unreal.StaticMeshComponent):
        if 'Drill' not in c.get_name() and 'Pickaxe' not in c.get_name():continue
        items.append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,location=list(c.get_editor_property('relative_location').to_tuple()),scale=list(c.get_editor_property('relative_scale3d').to_tuple()),rotation=list(c.get_editor_property('relative_rotation').to_tuple()),visible=c.get_editor_property('visible'),hidden=c.get_editor_property('hidden_in_game'),owner_no_see=c.get_editor_property('owner_no_see'),parent=c.get_attach_parent().get_name() if c.get_attach_parent() else None))
    for stem in ['DrillRightMount','DrillLeftMount']:
        c=cdo.get_editor_property(stem);items.append(dict(name=stem,location=list(c.get_editor_property('relative_location').to_tuple()),scale=list(c.get_editor_property('relative_scale3d').to_tuple()),rotation=list(c.get_editor_property('relative_rotation').to_tuple())))
    report[path]=items
(out/'authored_before.json').write_text(json.dumps(report,indent=2),encoding='utf-8');unreal.log(json.dumps(report))
