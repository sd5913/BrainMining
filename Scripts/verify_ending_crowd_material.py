import unreal

path = '/Game/EndingEmergence/Crowd/Lite/M_CrowdMotion'
material = unreal.load_asset(path)
if not material:
    raise RuntimeError('Missing crowd material: ' + path)

lib = unreal.MaterialEditingLibrary
base = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
roughness = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_ROUGHNESS)
if not isinstance(base, unreal.MaterialExpressionVertexColor):
    raise RuntimeError('Crowd Base Color is not driven by Vertex Color')
if not isinstance(roughness, unreal.MaterialExpressionConstant):
    raise RuntimeError('Crowd Roughness is not a constant node')
if abs(roughness.get_editor_property('r') - 0.78) > 0.001:
    raise RuntimeError('Crowd Roughness is not 0.78')
if material.get_editor_property('shading_model') != unreal.MaterialShadingModel.MSM_DEFAULT_LIT:
    raise RuntimeError('Crowd material is not Default Lit')

unreal.log('ENDING_CROWD_MATERIAL_VERIFY: default-lit vertex-color base, roughness=%.2f' % roughness.get_editor_property('r'))
