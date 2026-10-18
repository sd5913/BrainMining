import unreal

paths = [
    '/Game/CollectionPrototype/Materials/M_CollectionSilhouette',
    '/Game/EndingEmergence/Materials/M_EE_Fisheye',
    '/Game/EndingEmergence/Materials/M_EE_ScaleZoom',
    '/Game/EndingEmergence/Materials/M_EE_PulseTransit',
    '/Game/EndingEmergence/Materials/M_EE_WetLens',
    '/Game/EndingEmergence/Materials/M_EE_Exhaustion',
]

for path in paths:
    material = unreal.load_asset(path)
    if not material:
        unreal.log_error('MISSING ' + path)
        continue
    unreal.log('MATERIAL ' + path)
    unreal.log('DOMAIN ' + str(material.get_editor_property('material_domain')))
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for index, expression in enumerate(expressions):
        if isinstance(expression, unreal.MaterialExpressionCustom):
            code = expression.get_editor_property('code')
            output_type = expression.get_editor_property('output_type')
            unreal.log('CUSTOM %d output=%s code=%s' % (index, str(output_type), code.replace('\n', ' | ')))

unreal.log('ANDROID_POSTPROCESS_INSPECTION_DONE')