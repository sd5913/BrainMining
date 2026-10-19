import unreal

PATH = '/Game/EndingEmergence/Crowd/Lite/M_CrowdMotion'
material = unreal.load_asset(PATH)
if not material:
    raise RuntimeError('Missing crowd motion material: ' + PATH)
lib = unreal.MaterialEditingLibrary

vertex = None
for expression in lib.get_material_expressions(material):
    if isinstance(expression, unreal.MaterialExpressionVertexColor):
        vertex = expression
        break
if not vertex:
    raise RuntimeError('M_CrowdMotion has no vertex color expression')

# Keep the VAT position animation in World Position Offset, while giving the
# baked character palette a real lit surface response in the JapaneseCity rig.
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
if not lib.connect_material_property(vertex, '', unreal.MaterialProperty.MP_BASE_COLOR):
    raise RuntimeError('Could not connect crowd vertex color to Base Color')

roughness = None
for expression in lib.get_material_expressions(material):
    if isinstance(expression, unreal.MaterialExpressionConstant) and abs(expression.get_editor_property('r') - 0.78) < 0.001:
        roughness = expression
        break
if not roughness:
    roughness = lib.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r', 0.78)
if not lib.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS):
    raise RuntimeError('Could not connect crowd roughness')

lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log('ENDING_CROWD_MATERIAL_UPGRADED: lit vertex-color palette, roughness=0.78')
