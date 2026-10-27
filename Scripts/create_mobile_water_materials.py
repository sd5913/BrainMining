import json
from pathlib import Path

import unreal


library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
folder = '/Game/Mobile/Water'
master_path = folder + '/M_MobileWater'
master = unreal.load_asset(master_path) if assets.does_asset_exist(master_path) else None
if not master:
    source = unreal.load_asset('/Game/HeavenPreview/Materials/M_HeavenClearWater')
    assert source
    master = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('M_MobileWater', folder, source)
assert master
for expression in library.get_material_expressions(master):
    if isinstance(expression, unreal.MaterialExpressionSingleLayerWaterMaterialOutput):
        library.delete_material_expression(master, expression)
    elif expression.get_editor_property('desc') == 'MobileWaterSurface':
        library.delete_material_expression(master, expression)
master.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
master.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
master.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
master.set_editor_property('two_sided', True)
master.set_editor_property('float_precision_mode', unreal.MaterialFloatPrecisionMode.MFPM_FULL)

parameters = {
    str(expression.get_editor_property('parameter_name')): expression
    for expression in library.get_material_expressions(master)
    if isinstance(expression, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter))
}
normal = library.get_material_property_input_node(master, unreal.MaterialProperty.MP_NORMAL)
assert normal
camera = library.create_material_expression(master, unreal.MaterialExpressionCameraVectorWS)
camera.set_editor_property('desc', 'MobileWaterSurface')


def surface_expression(code, output_type, inputs):
    expression = library.create_material_expression(master, unreal.MaterialExpressionCustom)
    expression.set_editor_property('desc', 'MobileWaterSurface')
    expression.set_editor_property('code', code)
    expression.set_editor_property('output_type', output_type)
    bindings = []
    for name in inputs:
        binding = unreal.CustomInput()
        binding.set_editor_property('input_name', name)
        bindings.append(binding)
    expression.set_editor_property('inputs', bindings)
    for name, node in inputs.items():
        assert library.connect_material_expressions(node, '', expression, name)
    return expression


emissive = surface_expression(
    'float rim=pow(1-saturate(abs(dot(normalize(Normal),normalize(View)))),3); '
    'float glint=pow(saturate(dot(normalize(Normal),normalize(float3(.3,.2,1)))),24); '
    'return Tint*(.35+rim*.65)+float3(.09,.14,.15)*glint;',
    unreal.CustomMaterialOutputType.CMOT_FLOAT3,
    {'Normal': normal, 'View': camera, 'Tint': parameters['SurfaceTint']})
opacity = surface_expression(
    'float rim=pow(1-saturate(abs(dot(normalize(Normal),normalize(View)))),3); '
    'return saturate(Amount+rim*.22);',
    unreal.CustomMaterialOutputType.CMOT_FLOAT1,
    {'Normal': normal, 'View': camera, 'Amount': parameters['SurfaceOpacity']})
assert library.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert library.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
library.recompile_material(master)
assert assets.save_loaded_asset(master)

report = {'master': master_path, 'instances': {}}
for name, tint, settings in (
    ('MI_MobileBasin', (.035, .19, .23), {'SurfaceOpacity': .36, 'RippleStrength': .28, 'WaveAmplitude': 2.2, 'Roughness': .16, 'FlowStrength': 0}),
    ('MI_MobileOutflow', (.055, .24, .27), {'SurfaceOpacity': .44, 'RippleStrength': .33, 'WaveAmplitude': .22, 'Roughness': .18, 'FlowStrength': 1}),
    ('MI_MobileFlood', (.035, .13, .115), {'SurfaceOpacity': .52, 'RippleStrength': .23, 'WaveAmplitude': 0, 'Roughness': .2, 'FlowStrength': .25}),
):
    path = folder + '/' + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if not instance:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, master)
    library.set_material_instance_vector_parameter_value(instance, 'SurfaceTint', unreal.LinearColor(*tint, 1))
    for parameter, value in settings.items():
        library.set_material_instance_scalar_parameter_value(instance, parameter, value)
    library.update_material_instance(instance)
    assert assets.save_loaded_asset(instance)
    report['instances'][path] = settings

diagnostic_path = folder + '/M_WaterVisibilityDiagnostic'
diagnostic = unreal.load_asset(diagnostic_path) if assets.does_asset_exist(diagnostic_path) else None
if not diagnostic:
    diagnostic = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_WaterVisibilityDiagnostic', folder, unreal.Material, unreal.MaterialFactoryNew())
library.delete_all_material_expressions(diagnostic)
diagnostic.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
diagnostic.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
diagnostic.set_editor_property('two_sided', True)
color = library.create_material_expression(diagnostic, unreal.MaterialExpressionConstant3Vector)
color.set_editor_property('constant', unreal.LinearColor(0, .7, .7, 1))
assert library.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
library.recompile_material(diagnostic)
assert assets.save_loaded_asset(diagnostic)
report['diagnostic'] = diagnostic_path
report['shading_model'] = str(master.get_editor_property('shading_model'))
report['blend_mode'] = str(master.get_editor_property('blend_mode'))
report['two_sided'] = master.get_editor_property('two_sided')
report['float_precision_mode'] = str(master.get_editor_property('float_precision_mode'))
output = Path(unreal.Paths.project_saved_dir()) / 'Validation/MobileRepairs-20261025'
output.mkdir(parents=True, exist_ok=True)
(output / 'water-assets.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('MOBILE_WATER_ASSETS_READY')
unreal.SystemLibrary.quit_editor()
