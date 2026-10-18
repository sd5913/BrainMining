"""Replace desktop-only scene sampling in post-process Custom nodes with mobile-safe scene passthroughs."""
import unreal

materials = [
    ('/Game/CollectionPrototype/Materials/M_CollectionSilhouette', 'return Base.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
    ('/Game/EndingEmergence/Materials/M_EE_Fisheye', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
    ('/Game/EndingEmergence/Materials/M_EE_ScaleZoom', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
    ('/Game/EndingEmergence/Materials/M_EE_PulseTransit', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3),
    ('/Game/EndingEmergence/Materials/M_EE_WetLens', 'return SC;', unreal.CustomMaterialOutputType.CMOT_FLOAT4),
    ('/Game/EndingEmergence/Materials/M_EE_Exhaustion', 'return SC;', unreal.CustomMaterialOutputType.CMOT_FLOAT4),
]

for path, fallback, output_type in materials:
    material = unreal.load_asset(path)
    if not material:
        raise RuntimeError('Missing material: ' + path)
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    patched = 0
    for expression in expressions:
        if not isinstance(expression, unreal.MaterialExpressionCustom):
            continue
        code = expression.get_editor_property('code')
        if 'SceneTextureLookup' not in code:
            continue
        expression.set_editor_property('code', fallback)
        expression.set_editor_property('output_type', output_type)
        patched += 1
    if patched == 0:
        raise RuntimeError('No SceneTextureLookup Custom nodes found: ' + path)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Failed to save material: ' + path)
    unreal.log('PATCHED %s nodes=%d fallback=%s' % (path, patched, fallback))

unreal.log('ANDROID_POSTPROCESS_MATERIALS_DIRECT_FALLBACK_READY')