"""Readable glass for the new miniatures; use the supplied flower mark unchanged."""
import unreal
ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary;folder='/Game/CollectionPrototype/Materials';name='M_CollectionBranchGlass'
m=unreal.load_asset(folder+'/'+name) if EA.does_asset_exist(folder+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
ML.delete_all_material_expressions(m);m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
def node(cls,**values):
 n=ML.create_material_expression(m,cls)
 for k,v in values.items():n.set_editor_property(k,v)
 return n
f=node(unreal.MaterialExpressionFresnel,exponent=3.,base_reflect_fraction=.06)
c=node(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.22,.47,.52,1));assert ML.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
x=node(unreal.MaterialExpressionMultiply,const_b=.32);assert ML.connect_material_expressions(f,'',x,'A')
y=node(unreal.MaterialExpressionAdd,const_b=.035);assert ML.connect_material_expressions(x,'',y,'A');assert ML.connect_material_property(y,'',unreal.MaterialProperty.MP_OPACITY)
ML.recompile_material(m);assert EA.save_loaded_asset(m)
# SceneColor HDR already contains premultiplied translucent/additive colour.
# Standard translucent UI multiplied it by opacity again, erasing additive halos.
preview=unreal.load_asset('/Game/CollectionPrototype/UI/Redesign/M_CollectionPreview')
preview.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
ML.recompile_material(preview);assert EA.save_loaded_asset(preview)
name='M_CollectionCocktailLiquid'
m=unreal.load_asset(folder+'/'+name) if EA.does_asset_exist(folder+'/'+name) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
ML.delete_all_material_expressions(m);m.set_editor_property('two_sided',True);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
pos=node(unreal.MaterialExpressionWorldPosition);time=node(unreal.MaterialExpressionScalarParameter,parameter_name='AnimationTime',default_value=0.)
c=node(unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,code='float w=sin(P.x*.32+T*2.)*sin(P.y*.27-T*1.4); float r=pow(saturate(.5+.5*w),9.); return float3(.65,.055,.16)+float3(.32,.16,.08)*r;')
ins=[]
for name in ['P','T']:
 i=unreal.CustomInput();i.set_editor_property('input_name',name);ins.append(i)
c.set_editor_property('inputs',ins);assert ML.connect_material_expressions(pos,'XYZ',c,'P');assert ML.connect_material_expressions(time,'',c,'T');assert ML.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
alpha=node(unreal.MaterialExpressionConstant,r=.68);assert ML.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
ML.recompile_material(m);assert EA.save_loaded_asset(m)
unreal.SystemLibrary.quit_editor()
