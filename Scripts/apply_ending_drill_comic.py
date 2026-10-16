"""Ending-only inked rock material. Does not rebuild the map or edit shared tools."""
import unreal

lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/EndingEmergence/Materials'
name = 'M_EE_ComicRock'
mat = unreal.load_asset(folder+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(folder+'/'+name) else assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_instanced_static_meshes', True)
custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
bindings = [('P', unreal.MaterialExpressionWorldPosition, 'XYZ'), ('N', unreal.MaterialExpressionPixelNormalWS, ''), ('V', unreal.MaterialExpressionCameraVectorWS, ''), ('Tint', unreal.MaterialExpressionVectorParameter, ''), ('Depth', unreal.MaterialExpressionPixelDepth, ''), ('UV', unreal.MaterialExpressionTextureCoordinate, ''), ('C', unreal.MaterialExpressionVertexColor, ''), ('MeshInk', unreal.MaterialExpressionScalarParameter, ''), ('WorldScale', unreal.MaterialExpressionScalarParameter, ''), ('Reveal', unreal.MaterialExpressionScalarParameter, '')]
inputs = []
for key, cls, output in bindings:
    inp = unreal.CustomInput(); inp.set_editor_property('input_name', key); inputs.append(inp)
custom.set_editor_property('inputs', inputs)
for key, cls, output in bindings:
    node = lib.create_material_expression(mat, cls)
    if key == 'Tint':
        node.set_editor_property('parameter_name', 'Tint')
        node.set_editor_property('default_value', unreal.LinearColor(.12, .52, .65, 1))
    if key == 'MeshInk':
        node.set_editor_property('parameter_name', 'MeshInk')
        node.set_editor_property('default_value', 0.)
    if key in ('WorldScale', 'Reveal'):
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('default_value', 1.)
    assert lib.connect_material_expressions(node, output, custom, key)
custom.set_editor_property('code', r'''
float3 n=normalize(N),v=normalize(V);
// A soft camera light gives the cut volume depth; retain mostly continuous shade.
float depth=Depth/max(1,WorldScale);
float3 toLight=v*(depth/max(.1,-v.x))+float3(75,-115,145);
float3 key=normalize(toLight);
float diffuse=saturate(dot(n,key));
float band=.18+.82*smoothstep(.12,.78,diffuse);
float light=lerp(pow(diffuse,1.65),band,.08);
float attenuation=1/(1+pow(length(toLight)/360,2));
float focus=.045+.955*pow(saturate(dot(v,float3(-1,0,0))),10);
float3 illumination=float3(.018,.027,.045)+float3(2.25,1.95,1.55)*light*attenuation*focus;
float3 bary=float3(UV.x,UV.y,1-UV.x-UV.y);
float3 pixelDistance=bary/max(fwidth(bary),.00001);
float3 edgeInk=(1-smoothstep(.30,1.45,pixelDistance))*C.rgb;
float ink=MeshInk*max(edgeInk.x,max(edgeInk.y,edgeInk.z));
float rim=(1-smoothstep(.015,.075,abs(dot(n,v))))*.32;
ink=max(ink*.92,rim);
// Restrained pigment variation, not wallpaper dots or curved contour stripes.
float grit=frac(sin(dot(floor(P*.19),float3(12.9898,78.233,37.719)))*43758.5453);
float pigment=.95+.05*grit;
float facetTone=.94+.06*frac(sin(dot(n,float3(13,47,23)))*437.3);
float3 col=Tint.rgb*illumination*facetTone*pigment*1.55;
// Tinted, varied crease accents instead of an equally black triangle wireframe.
return lerp(col,col*.07,ink)*Reveal;
''')
assert lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('ENDING_DRILL_COMIC_MATERIAL_READY')

# Ending-only tool variant: keep the model's painted colours and normal detail.
name = 'M_EE_ComicDrill'
drill = unreal.load_asset(folder+'/'+name) or assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(drill)
drill.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
drill.set_editor_property('two_sided', True)
base = lib.create_material_expression(drill, unreal.MaterialExpressionTextureSampleParameter2D)
base.set_editor_property('parameter_name', 'BaseColor')
base.set_editor_property('texture', unreal.load_asset('/Game/Drill/Stylized/T_DrillStylized_BaseColor'))
normal = lib.create_material_expression(drill, unreal.MaterialExpressionTextureSampleParameter2D)
normal.set_editor_property('parameter_name', 'Normal')
normal.set_editor_property('texture', unreal.load_asset('/Game/Drill/Stylized/T_DrillStylized_Normal'))
normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
assert lib.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
shade = lib.create_material_expression(drill, unreal.MaterialExpressionCustom)
shade.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs = []
for key in ['Base', 'N', 'V']:
    inp = unreal.CustomInput();inp.set_editor_property('input_name', key);inputs.append(inp)
shade.set_editor_property('inputs', inputs)
lib.connect_material_expressions(base, 'RGB', shade, 'Base')
for key, cls in [('N', unreal.MaterialExpressionPixelNormalWS), ('V', unreal.MaterialExpressionCameraVectorWS)]:
    lib.connect_material_expressions(lib.create_material_expression(drill, cls), '', shade, key)
shade.set_editor_property('code', r'''
float3 colour=lerp(Base.rgb,dot(Base.rgb,float3(.21,.72,.07)).xxx,.08);
float edge=(1-smoothstep(.025,.20,abs(dot(normalize(N),normalize(V)))))*.54;
return colour*(1-edge);
''')
lib.connect_material_property(shade, '', unreal.MaterialProperty.MP_BASE_COLOR)
fill=lib.create_material_expression(drill, unreal.MaterialExpressionMultiply)
fill.set_editor_property('const_b', .12)
lib.connect_material_expressions(shade, '', fill, 'A')
lib.connect_material_property(fill, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,.48),(unreal.MaterialProperty.MP_METALLIC,.35),(unreal.MaterialProperty.MP_SPECULAR,.35)]:
    node=lib.create_material_expression(drill, unreal.MaterialExpressionConstant);node.set_editor_property('r',value)
    lib.connect_material_property(node,'',prop)
lib.recompile_material(drill)
assert unreal.EditorAssetLibrary.save_loaded_asset(drill)
unreal.log('ENDING_COMIC_DRILL_MATERIAL_READY')
