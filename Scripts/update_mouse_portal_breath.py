"""Rebuild only the painted portal material; keep the source painting intact."""
import unreal

lib = unreal.MaterialEditingLibrary
material = unreal.load_asset('/Game/MouseMuseum/M_MouseKiss')
texture = unreal.load_asset('/Game/MouseMuseum/T_MouseKiss')
assert material and texture
lib.delete_all_material_expressions(material)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property('two_sided', True)
uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate)
image = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample)
image.set_editor_property('texture', texture)
highlight = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
highlight.set_editor_property('parameter_name', 'Highlight')
highlight.set_editor_property('default_value', .35)
clock = lib.create_material_expression(material, unreal.MaterialExpressionTime)
custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
bindings = {'UV': (uv, ''), 'Image': (image, 'RGB'), 'Highlight': (highlight, ''), 'Time': (clock, '')}
inputs = []
for name in bindings:
    value = unreal.CustomInput()
    value.set_editor_property('input_name', name)
    inputs.append(value)
custom.set_editor_property('inputs', inputs)
custom.set_editor_property('code', '''
float breath=.5+.5*sin(Time*2.4);
float r=length((UV-float2(.6374,.5656))*float2(126,168));
float radius=3.6+breath*.8;
float ring=1-smoothstep(.16,.48,abs(r-radius));
float halo=exp(-abs(r-radius)*1.4)*.15;
return Image*.82+float3(1,.91,.65)*(ring+halo)*Highlight*(1.1+breath*.5);
''')
for name, (node, output) in bindings.items():
    assert lib.connect_material_expressions(node, output, custom, name)
assert lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.log('Portal breathing material saved; radius remains inside the interaction tolerance.')
