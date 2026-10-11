"""Build editable, softly lit drill dust using the supplied VFX pack's texture."""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game/Drill/Dust'], True)
texture = unreal.load_asset('/Game/Drill/Dust/T_Dust_Particle_D')
assert texture, 'Missing supplied dust texture'
export = unreal.AssetExportTask()
export.object = texture
export.filename = str(root/'SourceAssets/Drill/T_DrillDust.png')
export.automated = True
export.prompt = False
export.replace_identical = True
export.exporter = unreal.TextureExporterPNG()
assert unreal.Exporter.run_asset_export_task(export)

tools = unreal.AssetToolsHelpers.get_asset_tools()
material = unreal.load_asset('/Game/Drill/M_DrillDust')
if not material:
    material = tools.create_asset('M_DrillDust', '/Game/Drill', unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('two_sided', True)
material.set_editor_property('used_with_instanced_static_meshes', True)
material.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

def node(cls, x, y, **properties):
    result = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    for name, value in properties.items():
        result.set_editor_property(name, value)
    return result

def wire(a, output, b, input):
    assert unreal.MaterialEditingLibrary.connect_material_expressions(a, output, b, input)

tint = node(unreal.MaterialExpressionVectorParameter, -300, -300, parameter_name='SoilTint', default_value=unreal.LinearColor(0.22, 0.16, 0.09, 1))
unreal.MaterialEditingLibrary.connect_material_property(tint, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
roughness = node(unreal.MaterialExpressionConstant, -300, -450, r=1)
unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
mask = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name='DustTexture', texture=texture)
uv = node(unreal.MaterialExpressionTextureCoordinate, -1700, -200)
uv_scale = node(unreal.MaterialExpressionMultiply, -1500, -200, const_b=0.5)
wire(uv, '', uv_scale, 'A')
tile_u = node(unreal.MaterialExpressionPerInstanceCustomData, -1700, -400, data_index=1)
tile_v = node(unreal.MaterialExpressionPerInstanceCustomData, -1700, -600, data_index=2)
offset = node(unreal.MaterialExpressionAppendVector, -1500, -400)
wire(tile_u, '', offset, 'A')
wire(tile_v, '', offset, 'B')
offset_pixel = node(unreal.MaterialExpressionVertexInterpolator, -1300, -400)
wire(offset, '', offset_pixel, 'VS')
tile_uv = node(unreal.MaterialExpressionAdd, -1100, -200)
wire(uv_scale, '', tile_uv, 'A')
wire(offset_pixel, '', tile_uv, 'B')
wire(tile_uv, '', mask, 'UVs')
alpha = node(unreal.MaterialExpressionPerInstanceCustomData, -900, 250, data_index=0)
pixel_alpha = node(unreal.MaterialExpressionVertexInterpolator, -700, 250)
wire(alpha, '', pixel_alpha, 'VS')
opacity = node(unreal.MaterialExpressionMultiply, -450, 30)
wire(mask, 'R', opacity, 'A')
wire(pixel_alpha, '', opacity, 'B')
depth_fade = node(unreal.MaterialExpressionDepthFade, -200, 30, fade_distance_default=40)
wire(opacity, '', depth_fade, 'Opacity')
depth = node(unreal.MaterialExpressionPixelDepth, -900, 500)
near = node(unreal.MaterialExpressionSubtract, -700, 500, const_b=35)
wire(depth, '', near, 'A')
divide = node(unreal.MaterialExpressionDivide, -500, 500, const_b=100)
wire(near, '', divide, 'A')
clamp = node(unreal.MaterialExpressionSaturate, -300, 500)
wire(divide, '', clamp, '')
final = node(unreal.MaterialExpressionMultiply, 50, 100)
wire(depth_fade, '', final, 'A')
wire(clamp, '', final, 'B')
unreal.MaterialEditingLibrary.connect_material_property(final, '', unreal.MaterialProperty.MP_OPACITY)
unreal.MaterialEditingLibrary.recompile_material(material)
assert assets.save_loaded_asset(material, False)
unreal.log('DRILL_DUST_CREATED '+material.get_path_name())
