import unreal
ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary;AT=unreal.AssetToolsHelpers.get_asset_tools()
base='/Game/Passages/Echo/ArtP2/StyledVFX'
for name,ring in [('M_SoftEnergy',False),('M_SoftImpactRing',True)]:
 m=unreal.load_asset(base+'/'+name) or AT.create_asset(name,base,unreal.Material,unreal.MaterialFactoryNew())
 ML.delete_all_material_expressions(m)
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
 def node(cls):return ML.create_material_expression(m,cls)
 color=node(unreal.MaterialExpressionVectorParameter);color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(.06,.62,1,1))
 gain=node(unreal.MaterialExpressionConstant);gain.set_editor_property('r',65. if ring else 40.)
 mul=node(unreal.MaterialExpressionMultiply);assert ML.connect_material_expressions(color,'',mul,'A');assert ML.connect_material_expressions(gain,'',mul,'B');assert ML.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 if ring:
  uv=node(unreal.MaterialExpressionTextureCoordinate);time=node(unreal.MaterialExpressionTime);shape=node(unreal.MaterialExpressionCustom)
  inputs=[]
  for n in ['UV','T']:
   i=unreal.CustomInput();i.set_editor_property('input_name',n);inputs.append(i)
  shape.set_editor_property('inputs',inputs);shape.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
  shape.set_editor_property('code','float2 p=UV-.5; float a=atan2(p.y,p.x); float r=length(p); float wobble=.006*sin(a*9+T*4)+.003*sin(a*17-T*7); float band=exp(-pow((r-.38-wobble)/.025,2)); return band*(.72+.28*sin(a*3-T*2)*sin(a*3-T*2));')
  assert ML.connect_material_expressions(uv,'',shape,'UV');assert ML.connect_material_expressions(time,'',shape,'T')
 else:
  f=node(unreal.MaterialExpressionFresnel);f.set_editor_property('exponent',2.5);f.set_editor_property('base_reflect_fraction',.04)
  shape=node(unreal.MaterialExpressionOneMinus);assert ML.connect_material_expressions(f,'',shape,'')
 alpha=node(unreal.MaterialExpressionScalarParameter);alpha.set_editor_property('parameter_name','Opacity');alpha.set_editor_property('default_value',.8)
 opacity=node(unreal.MaterialExpressionMultiply);assert ML.connect_material_expressions(shape,'',opacity,'A');assert ML.connect_material_expressions(alpha,'',opacity,'B');assert ML.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
 ML.recompile_material(m);assert EA.save_loaded_asset(m)
unreal.log('P2_IMPACT_MATERIALS_READY')
# Preserve the source flipbook and turbulence, but remove opaque black soot.
# Only this material copy is used by styled card fire; source and poison stay intact.
fire=unreal.load_asset(base+'/M_StyledFire') or EA.duplicate_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Materials/M_Fire_3',base+'/M_StyledFire')
assert fire
fire.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE)
fire.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
ML.delete_all_material_expressions(fire)
def fnode(cls):return ML.create_material_expression(fire,cls)
tex=fnode(unreal.MaterialExpressionParticleSubUV);tex.set_editor_property('texture',unreal.load_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Textures/T_Fire_6'))
particle=fnode(unreal.MaterialExpressionParticleColor)
warm=fnode(unreal.MaterialExpressionConstant3Vector);warm.set_editor_property('constant',unreal.LinearColor(1,.19,.025,1))
hot=fnode(unreal.MaterialExpressionConstant3Vector);hot.set_editor_property('constant',unreal.LinearColor(1,.82,.32,1))
mix=fnode(unreal.MaterialExpressionLinearInterpolate)
assert ML.connect_material_expressions(warm,'',mix,'A');assert ML.connect_material_expressions(hot,'',mix,'B');assert ML.connect_material_expressions(tex,'R',mix,'Alpha')
strength=fnode(unreal.MaterialExpressionMultiply);strength.set_editor_property('const_b',45.)
assert ML.connect_material_expressions(mix,'',strength,'A');assert ML.connect_material_property(strength,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
alpha=fnode(unreal.MaterialExpressionMultiply)
assert ML.connect_material_expressions(tex,'R',alpha,'A');assert ML.connect_material_expressions(particle,'A',alpha,'B')
fade=fnode(unreal.MaterialExpressionDepthFade);fade.set_editor_property('fade_distance_default',35.)
assert ML.connect_material_expressions(alpha,'',fade,'Opacity');assert ML.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
ML.recompile_material(fire);assert EA.save_loaded_asset(fire)
