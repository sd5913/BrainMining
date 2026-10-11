"""Update existing editable heat material without rebuilding the HUD or its layout."""
import json
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
folder = '/Game/UI/MiningGUI'
material = unreal.load_asset(folder + '/M_JetpackHeat')
instance = unreal.load_asset(folder + '/MI_JetpackHeat')
assert material and instance, 'Existing jetpack heat UI is required'
lib = unreal.MaterialEditingLibrary
expressions = [x for x in unreal.ObjectIterator(unreal.MaterialExpression) if x.get_outer() == material]
shader = next(x for x in expressions if isinstance(x, unreal.MaterialExpressionCustom))
saturation = next((x for x in expressions if isinstance(x, unreal.MaterialExpressionScalarParameter)
                   and str(x.get_editor_property('parameter_name')) == 'Saturation'), None)
if not saturation:
    saturation = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -750, 1080)
    saturation.set_editor_property('parameter_name', 'Saturation')
saturation.set_editor_property('default_value', 1.1)
inputs = list(shader.get_editor_property('inputs'))
if not any(str(x.get_editor_property('input_name')) == 'Saturation' for x in inputs):
    entry = unreal.CustomInput()
    entry.set_editor_property('input_name', 'Saturation')
    inputs.append(entry)
    shader.set_editor_property('inputs', inputs)
assert lib.connect_material_expressions(saturation, '', shader, 'Saturation')
shader.set_editor_property('description', 'Heat fills left-to-right; locked red retreats during cooldown; editable saturation')
shader.set_editor_property('code', '''
float2 sampleUV = lerp(ArtworkUVRect.xy, ArtworkUVRect.zw, UV);
float4 artwork = Texture2DSample(BarTexture, BarTextureSampler, sampleUV);
float3 ramp = UV.x < 0.5 ? lerp(ColdGreen, WarmYellow, UV.x * 2.0)
                              : lerp(WarmYellow, HotRed, (UV.x - 0.5) * 2.0);
float3 activeColor = lerp(ramp, HotRed, saturate(Overheated));
float luminance = dot(activeColor, float3(0.2126, 0.7152, 0.0722));
activeColor = saturate(lerp(luminance.xxx, activeColor, max(Saturation, 0.0)));
float reveal = (Heat > 0.0 && UV.x <= saturate(Heat)) ? 1.0 : 0.0;
float3 tint = lerp(float3(1,1,1), activeColor, reveal);
// This white artwork stores some inter-segment gaps as opaque black pixels.
// Use its white coverage as an opacity mask so gaps stay transparent.
float coverage = artwork.a * dot(artwork.rgb, float3(0.2126, 0.7152, 0.0722));
return float4(tint, coverage);
''')
# Opacity must read alpha only. R defaults true and would wash out colored segments.
for expression in expressions:
    if isinstance(expression, unreal.MaterialExpressionComponentMask) and expression.get_editor_property('a'):
        for channel in ['r', 'g', 'b']:
            expression.set_editor_property(channel, False)
        expression.set_editor_property('a', True)
lib.recompile_material(material)

def color(r, g, b):
    def linear(v):
        v /= 255.
        return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
    return unreal.LinearColor(linear(r), linear(g), linear(b), 1.)

lib.set_material_instance_scalar_parameter_value(instance, 'Saturation', 1.1)
for parameter, value in [('ColdGreen', color(158, 243, 118)), ('WarmYellow', color(255, 215, 75)),
                         ('HotRed', color(244, 55, 31))]:
    lib.set_material_instance_vector_parameter_value(instance, parameter, value)
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
assert assets.save_loaded_asset(material, False)
assert assets.save_loaded_asset(instance, False)
out = Path(unreal.Paths.project_saved_dir()) / 'Jetpack/CoolingMaterial.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'passed': True, 'material': instance.get_path_name(), 'saturation': 1.1,
                          'opacity': 'white artwork coverage; alpha output only', 'cooldown': 'red reveal retreats to white'}, indent=2), encoding='utf-8')
unreal.log('JETPACK_COOLING_MATERIAL_READY')
