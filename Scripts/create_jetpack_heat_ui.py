"""Add only the editable heat bar to the artist's existing Mining HUD."""
import json
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)

root = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
folder = '/Game/UI/MiningGUI'
for file, name in [('10.png', 'T_JetpackHeatWhite'), ('11.png', 'T_JetpackHeatGradientReference')]:
    if not unreal.load_asset(folder + '/Art/' + name):
        task = unreal.AssetImportTask()
        task.filename = str(root / 'SourceAssets/JetpackUI' / file)
        task.destination_path = folder + '/Art'
        task.destination_name = name
        task.automated = True
        task.save = True
        task.replace_existing = False
        tools.import_asset_tasks([task])
    texture = unreal.load_asset(folder + '/Art/' + name)
    assert texture
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    assets.save_loaded_asset(texture, False)

material = unreal.load_asset(folder + '/M_JetpackHeat')
if not material:
    material = tools.create_asset('M_JetpackHeat', folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    lib = unreal.MaterialEditingLibrary
    def node(cls, x, y, **props):
        result = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items(): result.set_editor_property(key, value)
        return result
    def color(r, g, b):
        def linear(v):
            v /= 255.
            return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
        return unreal.LinearColor(linear(r), linear(g), linear(b), 1.)
    texture = node(unreal.MaterialExpressionTextureObjectParameter, -750, -400,
                   parameter_name='BarTexture', texture=unreal.load_asset(folder + '/Art/T_JetpackHeatWhite'))
    uv = node(unreal.MaterialExpressionTextureCoordinate, -750, -200)
    heat = node(unreal.MaterialExpressionScalarParameter, -750, 0, parameter_name='Heat', default_value=0.)
    locked = node(unreal.MaterialExpressionScalarParameter, -750, 180, parameter_name='Overheated', default_value=0.)
    green = node(unreal.MaterialExpressionVectorParameter, -750, 360, parameter_name='ColdGreen', default_value=color(165, 241, 150))
    yellow = node(unreal.MaterialExpressionVectorParameter, -750, 540, parameter_name='WarmYellow', default_value=color(255, 216, 102))
    red = node(unreal.MaterialExpressionVectorParameter, -750, 720, parameter_name='HotRed', default_value=color(195, 42, 25))
    crop = node(unreal.MaterialExpressionVectorParameter, -750, 900, parameter_name='ArtworkUVRect',
                default_value=unreal.LinearColor(66/3000, 446/1080, 2928/3000, 649/1080))
    shader = node(unreal.MaterialExpressionCustom, -200, -200,
                  description='Original segmented artwork, left-to-right heat reveal, full-red lockout',
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ['BarTexture', 'UV', 'Heat', 'Overheated', 'ColdGreen', 'WarmYellow', 'HotRed', 'ArtworkUVRect']:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    shader.set_editor_property('inputs', inputs)
    shader.set_editor_property('code', '''
float2 sampleUV = lerp(ArtworkUVRect.xy, ArtworkUVRect.zw, UV);
float4 artwork = Texture2DSample(BarTexture, BarTextureSampler, sampleUV);
float3 ramp = UV.x < 0.5 ? lerp(ColdGreen, WarmYellow, UV.x * 2.0)
                              : lerp(WarmYellow, HotRed, (UV.x - 0.5) * 2.0);
float reveal = (Heat > 0.0 && UV.x <= saturate(Heat)) ? 1.0 : 0.0;
float3 tint = lerp(float3(1,1,1), ramp, reveal);
tint = lerp(tint, HotRed, saturate(Overheated));
return float4(artwork.rgb * tint, artwork.a);
''')
    for expr, name, output in [(texture, 'BarTexture', ''), (uv, 'UV', ''), (heat, 'Heat', ''),
            (locked, 'Overheated', ''), (green, 'ColdGreen', 'RGB'), (yellow, 'WarmYellow', 'RGB'),
            (red, 'HotRed', 'RGB'), (crop, 'ArtworkUVRect', 'RGBA')]:
        assert lib.connect_material_expressions(expr, output, shader, name)
    rgb = node(unreal.MaterialExpressionComponentMask, 150, -200, r=True, g=True, b=True)
    alpha = node(unreal.MaterialExpressionComponentMask, 150, 0, a=True)
    assert lib.connect_material_expressions(shader, '', rgb, '')
    assert lib.connect_material_expressions(shader, '', alpha, '')
    assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    assert assets.save_loaded_asset(material, False)

instance = unreal.load_asset(folder + '/MI_JetpackHeat')
if not instance:
    instance = tools.create_asset('MI_JetpackHeat', folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, material)
    assert assets.save_loaded_asset(instance, False)
assert unreal.MiningGUIEditorLibrary.add_jetpack_heat_overlay()
for name in ['WBP_JetpackHeat', 'WBP_MiningHUD']:
    bp = unreal.load_asset(folder + '/' + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, False)
out = root / 'Saved/Jetpack/assets.json'
out.write_text(json.dumps({'passed': True, 'widget': folder + '/WBP_JetpackHeat', 'hud': folder + '/WBP_MiningHUD',
    'material': folder + '/MI_JetpackHeat', 'artwork': ['10.png', '11.png']}, indent=2), encoding='utf-8')
unreal.log('JETPACK_HEAT_UI_READY')
