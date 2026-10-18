import json,unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/PackagePlaytest/ending-drill-assets.json'
report={}
for path in ['/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter','/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/MainGame/Blueprints/BP_MainGameCharacter']:
    cdo=unreal.get_default_object(unreal.load_asset(path).generated_class())
    parts=[]
    for c in cdo.get_components_by_class(unreal.SceneComponent):
        if 'Drill' not in c.get_name():continue
        record={'name':c.get_name(),'transform':str(c.get_relative_transform()),'visible':c.is_visible(),'hidden':c.get_editor_property('hidden_in_game')}
        if isinstance(c,unreal.StaticMeshComponent):
            mesh=c.static_mesh
            record.update(mesh=mesh.get_path_name() if mesh else None,bounds=str(mesh.get_bounding_box()) if mesh else None)
        parts.append(record)
    report[path]=parts
m=unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_ComicDrill')
report['material']={'blend':str(m.get_editor_property('blend_mode')),'clip':m.get_editor_property('opacity_mask_clip_value'),'expressions':[]}
for n in unreal.MaterialEditingLibrary.get_material_expressions(m):
    if isinstance(n,unreal.MaterialExpressionCustom):report['material']['expressions'].append(n.get_editor_property('code'))
    if isinstance(n,unreal.MaterialExpressionScalarParameter):report['material'][str(n.get_editor_property('parameter_name'))]=n.get_editor_property('default_value')
out.write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
