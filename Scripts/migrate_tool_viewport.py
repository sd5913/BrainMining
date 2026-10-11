"""One-time migration of the existing artist pose to editable native tool components."""
import unreal, json
from pathlib import Path

bp=unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter')
assert bp
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo=unreal.get_default_object(bp.generated_class())
effects=cdo.get_component_by_class(unreal.MiningToolEffectsComponent)
subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library=unreal.SubobjectDataBlueprintFunctionLibrary
objects={}
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data=library.get_data(handle)
    obj=library.get_object_for_blueprint(data,bp)
    if obj:objects[obj.get_name()]=obj
names=['PickaxeMount','PickaxeMotion','PickaxeModel','DrillRightMount','DrillRightMotion','DrillRightModel','DrillLeftMount','DrillLeftMotion','DrillLeftModel']
assert all(name in objects for name in names),list(objects)
before={name:str(effects.get_editor_property(name)) for name in ['drill_location','left_drill_location','drill_rotation','left_drill_rotation','drill_scale','drill_dust_strength','drill_dust_color']}
if unreal.EditorAssetLibrary.get_metadata_tag(bp,'MiningToolViewportLayout')!='1':
    for side,prefix in [('Right','drill'),('Left','left_drill')]:
        mount=objects['Drill'+side+'Mount']
        mount.set_editor_property('relative_location',effects.get_editor_property(prefix+'_location'))
        mount.set_editor_property('relative_rotation',effects.get_editor_property(prefix+'_rotation'))
        model=objects['Drill'+side+'Model'];scale=effects.drill_scale
        model.set_editor_property('relative_scale3d',unreal.Vector(scale,-scale if side=='Left' else scale,scale))
        model.set_static_mesh(effects.get_editor_property('drill_mesh_asset'))
    objects['PickaxeModel'].set_static_mesh(effects.get_editor_property('pickaxe_mesh_asset'))
    unreal.EditorAssetLibrary.set_metadata_tag(bp,'MiningToolViewportLayout','1')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
cdo=unreal.get_default_object(bp.generated_class())
report={'passed':True,'legacy_settings_preserved':before,'components':{}}
for name in names:
    component=cdo.get_editor_property(name)
    report['components'][name]={'path':component.get_path_name(),'pose':str(component.get_editor_property('relative_location')),'rotation':str(component.get_editor_property('relative_rotation')),'scale':str(component.get_editor_property('relative_scale3d'))}
out=Path(unreal.Paths.project_dir(),'Saved/Validation/ToolViewport');out.mkdir(parents=True,exist_ok=True)
(out/'migration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('TOOL_VIEWPORT_MIGRATION '+json.dumps(report))
