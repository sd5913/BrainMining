"""Create an editable arc material and mount the artist's original warning PNG."""
import json
from pathlib import Path
import unreal

unreal.EditorPythonScripting.set_keep_python_script_alive(True)
root = Path(unreal.Paths.project_dir())
folder = '/Game/UI/MiningGUI/JetpackGauge'
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
lib = unreal.MaterialEditingLibrary

warning_path = '/Game/UI/MiningGUI/Art/T_JetpackHeatWarning'
warning = unreal.load_asset(warning_path)
if not warning:
    task = unreal.AssetImportTask()
    task.filename = str(root / 'SourceAssets/JetpackUI/JetpackHeatWarning.png')
    task.destination_path = '/Game/UI/MiningGUI/Art'
    task.destination_name = 'T_JetpackHeatWarning'
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    warning = unreal.load_asset(warning_path)
assert warning
warning.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
warning.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
warning.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
warning.set_editor_property('never_stream', True)
assert assets.save_loaded_asset(warning, False)

def color(r, g, b):
    def linear(v):
        v /= 255.
        return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
    return unreal.LinearColor(linear(r), linear(g), linear(b), 1.)

# The 300 x 120 canvas crops the unused lower half of a circular dashboard arc.
# Its X/Y mapping preserves a circular radius at this canvas aspect ratio.
gauge_code = '''
float2 p = float2((UV.x - 0.5) * 2.0, UV.y * 0.8 - 1.0);
float r = length(p);
float angle = atan2(p.x, -p.y);
float sweep = clamp(SweepDegrees, 60.0, 170.0) * 0.01745329252;
float t = (angle + sweep * 0.5) / sweep;
float aa = max(fwidth(r), 0.001);
float angularAA = max(fwidth(t), 0.001);
float d = abs(r - Radius);
float angular = smoothstep(-angularAA, angularAA, t) * (1.0-smoothstep(1.0-angularAA, 1.0+angularAA, t));
float shape = (1.0-smoothstep(Thickness+0.014-aa, Thickness+0.014+aa, d)) * angular;
float inner = 1.0-smoothstep(Thickness-aa, Thickness+aa, d);
float threshold = clamp(WarningThreshold, 0.1, 0.95);
float3 zone = t < threshold ? YellowColor : RedColor;
float3 groove = TrackColor * (0.72 + 0.28*saturate(1.0-d/max(Thickness,0.01)));
// Keep a subtle red terminal zone and a threshold notch in the empty groove.
groove = t >= threshold ? lerp(groove, RedColor*0.18, 0.35) : groove;
float filled = Heat > 0.0 ? 1.0-smoothstep(saturate(Heat)-angularAA, saturate(Heat)+angularAA, t) : 0.0;
float3 interior = lerp(groove, zone, filled);
float notch = (1.0-smoothstep(0.004,0.010,abs(t-threshold))) * inner;
interior = lerp(interior, OutlineColor, notch*0.8);
float3 rim = lerp(OutlineColor, RedColor*0.35, saturate(Overheated));
return float4(lerp(rim,interior,inner),shape);
'''

material = unreal.load_asset(folder + '/M_JetpackArc')
if not material:
    material = tools.create_asset('M_JetpackArc', folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    def node(cls, x, y, **props):
        obj = lib.create_material_expression(material, cls, x, y)
        for key, value in props.items(): obj.set_editor_property(key, value)
        return obj
    uv = node(unreal.MaterialExpressionTextureCoordinate, -800, -200)
    parameters = [
        ('Heat', 0.), ('Overheated', 0.), ('WarningThreshold', .75),
        ('Radius', .83), ('Thickness', .035), ('SweepDegrees', 140.),
        ('TrackColor', color(35, 39, 44)), ('YellowColor', color(255, 196, 48)),
        ('RedColor', color(249, 54, 40)), ('OutlineColor', color(9, 12, 16))]
    nodes = []
    for index, (name, value) in enumerate(parameters):
        cls = unreal.MaterialExpressionVectorParameter if isinstance(value, unreal.LinearColor) else unreal.MaterialExpressionScalarParameter
        nodes.append(node(cls, -800, index*140, parameter_name=name, default_value=value))
    custom = node(unreal.MaterialExpressionCustom, -200, -100,
                  description='Bottom-center dashboard; yellow first 75%, red final 25%; left-to-right heat fill',
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ['UV'] + [p[0] for p in parameters]:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', gauge_code)
    assert lib.connect_material_expressions(uv, '', custom, 'UV')
    for (name, value), expression in zip(parameters, nodes):
        assert lib.connect_material_expressions(expression, 'RGB' if isinstance(value, unreal.LinearColor) else '', custom, name)
    rgb = node(unreal.MaterialExpressionComponentMask, 200, -100, r=True, g=True, b=True, a=False)
    alpha = node(unreal.MaterialExpressionComponentMask, 200, 100, r=False, g=False, b=False, a=True)
    assert lib.connect_material_expressions(custom, '', rgb, '')
    assert lib.connect_material_expressions(custom, '', alpha, '')
    assert lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)

# Update the existing material as well, without replacing artist color settings.
customs = [obj for obj in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if obj.get_outer() == material]
assert len(customs) == 1
customs[0].set_editor_property('code', gauge_code)
customs[0].set_editor_property('description', 'Bottom-center dashboard; yellow first 75%, red final 25%; left-to-right heat fill')
for expression in unreal.ObjectIterator(unreal.MaterialExpressionScalarParameter):
    if expression.get_outer() == material and str(expression.get_editor_property('parameter_name')) == 'Thickness':
        expression.set_editor_property('default_value', .035)
lib.recompile_material(material)
assert assets.save_loaded_asset(material, False)

instance = unreal.load_asset(folder + '/MI_JetpackArc')
if not instance:
    instance = tools.create_asset('MI_JetpackArc', folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance, material)
    assert assets.save_loaded_asset(instance, False)
assert unreal.MiningGUIEditorLibrary.upgrade_jetpack_to_arc_gauge()

# Keep layout in the Designer assets; runtime code only animates heat and fading.
heat_bp = unreal.load_asset('/Game/UI/MiningGUI/WBP_JetpackHeat')
hud_bp = unreal.load_asset('/Game/UI/MiningGUI/WBP_MiningHUD')
bar = next(obj for obj in unreal.ObjectIterator(unreal.Image)
           if obj.get_path_name() == heat_bp.get_path_name() + ':WidgetTree.HeatBar')
bar.slot.set_position(unreal.Vector2D(0., 0.))
bar.slot.set_size(unreal.Vector2D(300., 120.))
bar.set_render_opacity(.8)
unreal.BlueprintEditorLibrary.compile_blueprint(heat_bp)
assert assets.save_loaded_asset(heat_bp, False)
gauge = next(obj for obj in unreal.ObjectIterator(unreal.MiningGUIJetpackHeatWidget)
             if obj.get_path_name() == hud_bp.get_path_name() + ':WidgetTree.JetpackHeatBar')
gauge.slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(.5, 1.), maximum=unreal.Vector2D(.5, 1.)))
gauge.slot.set_alignment(unreal.Vector2D(.5, 1.))
gauge.slot.set_position(unreal.Vector2D(0., -40.))
gauge.slot.set_size(unreal.Vector2D(300., 120.))
unreal.BlueprintEditorLibrary.compile_blueprint(hud_bp)
assert assets.save_loaded_asset(hud_bp, False)
out = root / 'Saved/Jetpack/ArcAssets.json'
out.write_text(json.dumps({'passed': True, 'widget': '/Game/UI/MiningGUI/WBP_JetpackHeat',
                          'material': instance.get_path_name(), 'warning': warning_path,
                          'yellow_fraction': .75, 'red_fraction': .25,
                          'automatic_visibility': True, 'position': 'bottom_center',
                          'fill_direction': 'left_to_right', 'canvas_size': [300, 120],
                          'bottom_offset': 40, 'opacity': .8, 'thickness': .035}, indent=2), encoding='utf-8')
unreal.log('JETPACK_ARC_UI_READY')
