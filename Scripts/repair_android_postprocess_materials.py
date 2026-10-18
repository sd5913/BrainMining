"""Make desktop-only post-process Custom nodes safe for Android shader compilation."""
import unreal

materials = [
    ('/Game/CollectionPrototype/Materials/M_CollectionSilhouette', 'return Base.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, False),
    ('/Game/EndingEmergence/Materials/M_EE_Fisheye', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, False),
    ('/Game/EndingEmergence/Materials/M_EE_ScaleZoom', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, False),
    ('/Game/EndingEmergence/Materials/M_EE_PulseTransit', 'return SC.rgb;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, False),
    ('/Game/EndingEmergence/Materials/M_EE_WetLens', 'return SC;', unreal.CustomMaterialOutputType.CMOT_FLOAT4, False),
    ('/Game/EndingEmergence/Materials/M_EE_Exhaustion', 'return SC;', unreal.CustomMaterialOutputType.CMOT_FLOAT4, False),
    ('/Game/CelPaint/Main/CelPaint', 'return ppinput0;', unreal.CustomMaterialOutputType.CMOT_FLOAT3, True),
]

for path, fallback, output_type, platform_guard in materials:
    material = unreal.load_asset(path)
    if not material:
        raise RuntimeError('Missing material: ' + path)

    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    patched = 0
    for expression in expressions:
        if not isinstance(expression, unreal.MaterialExpressionCustom):
            continue

        code = expression.get_editor_property('code')
        if platform_guard:
            if '#if ES3_1_PROFILE' in code:
                continue
            if 'GetDefaultSceneTextureUV' not in code and 'SceneTextureLookup' not in code:
                continue
            replacement = '#if ES3_1_PROFILE\n%s\n#else\n%s\n#endif' % (fallback, code)
        else:
            if code.strip() == fallback.strip():
                continue
            if 'SceneTextureLookup' not in code and 'GetDefaultSceneTextureUV' not in code:
                continue
            replacement = fallback

        expression.set_editor_property('code', replacement)
        expression.set_editor_property('output_type', output_type)
        patched += 1

    if patched == 0:
        unreal.log('SKIPPED %s: no matching Custom nodes' % path)
        continue

    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Failed to save material: ' + path)
    unreal.log('PATCHED %s nodes=%d fallback=%s guarded=%s' % (path, patched, fallback, platform_guard))

unreal.log('ANDROID_POSTPROCESS_MATERIALS_DIRECT_FALLBACK_READY')
