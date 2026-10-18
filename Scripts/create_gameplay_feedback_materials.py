"""Conductive arcs and an opaque miner palette with capture-safe ambient fill."""
import unreal
lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary
path='/Game/WenSi/Mirror/Materials'
for name,color in [('M_ConductiveArc',(.3,3.5,6,1))]:
    m=unreal.load_asset(path+'/'+name) if assets.does_asset_exist(path+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,path,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(*color))
    lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.recompile_material(m);assert assets.save_loaded_asset(m)
# Capture-only meshes do not receive the main view's Lumen surface-cache fill.
# A directional, opaque color fill keeps the miner legible in the dim capture.
name='M_MirrorMiner'
m=unreal.load_asset(path+'/'+name) if assets.does_asset_exist(path+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,path,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter)
tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(.62,.53,.4,1))
normal=lib.create_material_expression(m,unreal.MaterialExpressionPixelNormalWS)
fill=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
fill.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ['N','Tint']:
    item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
fill.set_editor_property('inputs',inputs)
fill.set_editor_property('code','return Tint * (2.4 + 1.6 * saturate(dot(normalize(N), normalize(float3(0.4,-0.5,0.8)))));')
lib.connect_material_expressions(normal,'',fill,'N');lib.connect_material_expressions(tint,'',fill,'Tint')
rough=lib.create_material_expression(m,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.8)
lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_BASE_COLOR)
lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
lib.connect_material_property(fill,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);assert assets.save_loaded_asset(m)
# Feather the egg's edges in the material, so only this reflection is softened.
name='M_MirrorEgg'
m=unreal.load_asset(path+'/'+name) if assets.does_asset_exist(path+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,path,unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(m)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
normal=lib.create_material_expression(m,unreal.MaterialExpressionPixelNormalWS)
view=lib.create_material_expression(m,unreal.MaterialExpressionCameraVectorWS)
edge=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
edge.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inputs=[]
for name in ['N','V']:
    item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
edge.set_editor_property('inputs',inputs)
edge.set_editor_property('code','return pow(smoothstep(0.0,0.85,saturate(dot(normalize(N),normalize(V)))),1.6);')
lib.connect_material_expressions(normal,'',edge,'N');lib.connect_material_expressions(view,'',edge,'V')
alpha=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter);alpha.set_editor_property('parameter_name','SoftOpacity');alpha.set_editor_property('default_value',.8)
opacity=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(edge,'',opacity,'A');lib.connect_material_expressions(alpha,'',opacity,'B')
tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,.88,.64,1))
strength=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter);strength.set_editor_property('parameter_name','Strength');strength.set_editor_property('default_value',6)
color=lib.create_material_expression(m,unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(tint,'',color,'A');lib.connect_material_expressions(strength,'',color,'B')
lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(m);assert assets.save_loaded_asset(m)
unreal.log('GAMEPLAY_FEEDBACK_MATERIALS_READY')
