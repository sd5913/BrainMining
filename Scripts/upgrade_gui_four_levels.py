"""Import original four-part artwork; update only existing upgrade ring layers."""
import json
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root = Path(unreal.Paths.project_dir())
folder = '/Game/UI/MiningGUI/FourLevel'
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
lib = unreal.MaterialEditingLibrary

for name in ['T_UpgradeRingFourWhite', 'T_UpgradeRingFourGreen']:
    if not unreal.load_asset(folder + '/' + name):
        task = unreal.AssetImportTask()
        task.filename = str(root / 'SourceAssets/MiningGUIFourLevel' / (name + '.png'))
        task.destination_path = folder
        task.destination_name = name
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
    texture = unreal.load_asset(folder + '/' + name)
    assert texture, name
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    assert assets.save_loaded_asset(texture, False)

material = unreal.load_asset(folder + '/M_UpgradeRingFour')
if not material:
    material = tools.create_asset('M_UpgradeRingFour', folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    def node(cls, x, y, **props):
        result = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items(): result.set_editor_property(key, value)
        return result
    texture = node(unreal.MaterialExpressionTextureObjectParameter, -650, -250,
                   parameter_name='Artwork', texture=unreal.load_asset(folder + '/T_UpgradeRingFourWhite'))
    uv = node(unreal.MaterialExpressionTextureCoordinate, -650, -50)
    quarter = node(unreal.MaterialExpressionScalarParameter, -650, 150,
                   parameter_name='QuarterTurn', default_value=0.)
    tint = node(unreal.MaterialExpressionVectorParameter, -650, 350,
                parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    crop = node(unreal.MaterialExpressionVectorParameter, -650, 550,
                parameter_name='ArtworkUVRect',
                default_value=unreal.LinearColor(303/3103, 318/3072, 2739/3103, 2754/3072))
    shader = node(unreal.MaterialExpressionCustom, -150, -100,
                  description='Original RGBA art; square UV crop and clockwise quadrant placement',
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ['Artwork', 'UV', 'QuarterTurn', 'Tint', 'ArtworkUVRect']:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    shader.set_editor_property('inputs', inputs)
    shader.set_editor_property('code', '''
float2 p = UV - 0.5;
int quarter = ((int)round(QuarterTurn) % 4 + 4) % 4;
if (quarter == 1) p = float2(p.y, -p.x);
else if (quarter == 2) p = -p;
else if (quarter == 3) p = float2(-p.y, p.x);
float2 sampleUV = lerp(ArtworkUVRect.xy, ArtworkUVRect.zw, p + 0.5);
float4 art = Texture2DSample(Artwork, ArtworkSampler, sampleUV);
return float4(Tint, art.a);
''')
    for expr, name, output in [(texture, 'Artwork', ''), (uv, 'UV', ''),
            (quarter, 'QuarterTurn', ''), (tint, 'Tint', 'RGB'), (crop, 'ArtworkUVRect', 'RGBA')]:
        assert lib.connect_material_expressions(expr, output, shader, name)
    rgb = node(unreal.MaterialExpressionComponentMask, 200, -100, r=True, g=True, b=True)
    alpha = node(unreal.MaterialExpressionComponentMask, 200, 80, r=False, g=False, b=False, a=True)
    assert lib.connect_material_expressions(shader, '', rgb, '')
    assert lib.connect_material_expressions(shader, '', alpha, '')
    assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    assert assets.save_loaded_asset(material, False)

for expression in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
    if expression.get_outer() == material:
        expression.set_editor_property('code', expression.get_editor_property('code').replace(
            'return float4(art.rgb * Tint, art.a);', 'return float4(Tint, art.a);'))
for expression in unreal.ObjectIterator(unreal.MaterialExpressionComponentMask):
    if expression.get_outer() == material and expression.get_editor_property('a'):
        # ComponentMask defaults R=true; opacity must read only the artwork alpha.
        for channel in ['r', 'g', 'b']: expression.set_editor_property(channel, False)
lib.recompile_material(material)
assert assets.save_loaded_asset(material, False)

def linear(value):
    value /= 255.
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
green = unreal.LinearColor(linear(79), linear(238), linear(80), 1.)
instances = [('MI_UpgradeRingWhite', 'T_UpgradeRingFourWhite', 0, unreal.LinearColor(1, 1, 1, 1)),
             ('MI_UpgradeRingLocked', 'T_UpgradeRingFourWhite', 0, unreal.LinearColor(.22, .22, .22, 1))]
instances += [('MI_UpgradeRingGreen' + str(i), 'T_UpgradeRingFourGreen', i-1, green) for i in range(1, 5)]
for name, art, quarter, tint in instances:
    instance = unreal.load_asset(folder + '/' + name)
    if not instance:
        instance = tools.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(instance, material)
    lib.set_material_instance_texture_parameter_value(instance, 'Artwork', unreal.load_asset(folder + '/' + art))
    lib.set_material_instance_scalar_parameter_value(instance, 'QuarterTurn', quarter)
    lib.set_material_instance_vector_parameter_value(instance, 'Tint', tint)
    assert assets.save_loaded_asset(instance, False)

assert unreal.MiningGUIEditorLibrary.upgrade_gui_to_four_levels(), 'Four-level widget update failed'
# Explicit paint order keeps the editable green overlays above the base ring.
for name in ['WBP_UpgradePickaxe', 'WBP_UpgradeVortex', 'WBP_UpgradeBlast']:
    bp = unreal.load_asset('/Game/UI/MiningGUI/' + name)
    images = {o.get_name(): o for o in unreal.ObjectIterator(unreal.Image)
              if name + '.' + name + ':WidgetTree.' in o.get_path_name()}
    top = max(images[x].slot.get_z_order() for x in ['RingGrey', 'LockedRing', 'ItemIcon', 'LockIcon'])
    for index in range(1, 5):
        images['GreenSegment' + str(index)].slot.set_z_order(top + index)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, False)
output = root / 'Saved/MiningGUI/FourLevelAssets.json'
output.write_text(json.dumps({'passed': True, 'max_level': 4,
    'folder': folder, 'instances': [x[0] for x in instances]}, indent=2), encoding='utf-8')
unreal.log('FOUR_LEVEL_UPGRADE_ASSETS_READY')
