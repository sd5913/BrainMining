"""Ending-only paper and physical computer screen materials. No shared assets changed."""
from pathlib import Path
import unreal
project=Path(unreal.Paths.project_dir())
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
folder='/Game/EndingEmergence/Materials'
task=unreal.AssetImportTask();task.filename=str(project/'SourceAssets/EndingEmergence/PaperFinale/T_EE_DeveloperCredits.png');task.destination_path='/Game/EndingEmergence/PaperFinale';task.destination_name='T_EE_DeveloperCredits';task.automated=True;task.replace_existing=True;task.save=True
assets.import_asset_tasks([task])
tex=unreal.load_asset('/Game/EndingEmergence/PaperFinale/T_EE_DeveloperCredits')
tex.set_editor_property('never_stream',True);tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);unreal.EditorAssetLibrary.save_loaded_asset(tex)
linear_path='/Game/EndingEmergence/PaperFinale/T_EE_CaveCapturePlaceholder'
linear_tex=unreal.load_asset(linear_path) or unreal.EditorAssetLibrary.duplicate_asset('/Game/EndingEmergence/PaperFinale/T_EE_DeveloperCredits',linear_path)
linear_tex.set_editor_property('srgb',False);unreal.EditorAssetLibrary.save_loaded_asset(linear_tex)
doodles=[]
for source in sorted((project/'SourceAssets/EndingEmergence/PaperFinale/Doodles').glob('T_EE_Doodle_*.png')):
 task=unreal.AssetImportTask();task.filename=str(source);task.destination_path='/Game/EndingEmergence/PaperFinale/Doodles';task.destination_name=source.stem;task.automated=True;task.replace_existing=True;task.save=True
 assets.import_asset_tasks([task])
 texture=unreal.load_asset(task.destination_path+'/'+source.stem)
 texture.set_editor_property('never_stream',True);texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_WORLD);texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
 texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE);texture.set_editor_property('filter',unreal.TextureFilter.TF_TRILINEAR)
 unreal.EditorAssetLibrary.save_loaded_asset(texture);doodles.append(texture)
assert len(doodles)==12, 'All twelve paper drawings must be imported'
rock_source=project/'SourceAssets/EndingEmergence/PaperFinale/Hero/T_EE_Doodle_rock.png'
task=unreal.AssetImportTask();task.filename=str(rock_source);task.destination_path='/Game/EndingEmergence/PaperFinale/Hero';task.destination_name=rock_source.stem;task.automated=True;task.replace_existing=True;task.save=True
assets.import_asset_tasks([task])
rock=unreal.load_asset(task.destination_path+'/'+rock_source.stem)
rock.set_editor_property('never_stream',True);rock.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
rock.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE);rock.set_editor_property('filter',unreal.TextureFilter.TF_TRILINEAR)
unreal.EditorAssetLibrary.save_loaded_asset(rock)
for name in ['PaperStock','DeveloperCredits','DoodleInk']:
 path=folder+'/M_EE_'+name
 mat=unreal.load_asset(path) or assets.create_asset('M_EE_'+name,folder,unreal.Material,unreal.MaterialFactoryNew())
 lib.delete_all_material_expressions(mat);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);mat.set_editor_property('two_sided',True);mat.set_editor_property('used_with_instanced_static_meshes',True)
 code=lib.create_material_expression(mat,unreal.MaterialExpressionCustom);code.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 bindings={}
 if name=='PaperStock':
  tint=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(.8,.75,.6,1));bindings['Tint']=(tint,'')
  bindings['P']=(lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition),'XYZ');bindings['N']=(lib.create_material_expression(mat,unreal.MaterialExpressionPixelNormalWS),'')
  source=r'''float grain=frac(sin(dot(floor(P*1.8),float3(12.989,78.233,37.719)))*43758.54);float shade=.65+.35*abs(dot(normalize(N),normalize(float3(-.5,-.3,1))));return Tint.rgb*shade*(.96+.04*grain);'''
 elif name=='DeveloperCredits':
  sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','Credits');sample.set_editor_property('texture',tex);bindings['Image']=(sample,'RGB')
  power=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter);power.set_editor_property('parameter_name','Power');power.set_editor_property('default_value',0.);bindings['Power']=(power,'')
  cave=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D);cave.set_editor_property('parameter_name','CaveImage');cave.set_editor_property('texture',linear_tex);cave.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR);bindings['Cave']=(cave,'RGB')
  mix=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter);mix.set_editor_property('parameter_name','CreditMix');mix.set_editor_property('default_value',0.);bindings['CreditMix']=(mix,'')
  source='return (Cave.rgb*(1-.55*CreditMix)+Image.rgb*CreditMix*1.25)*Power;'
 else:
  mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
  mat.set_editor_property('opacity_mask_clip_value',.15)
  sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','Doodle');sample.set_editor_property('texture',doodles[0]);bindings['Image']=(sample,'RGB')
  tint=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(.94,.92,.87,1));bindings['Tint']=(tint,'')
  bindings['N']=(lib.create_material_expression(mat,unreal.MaterialExpressionPixelNormalWS),'')
  source='float shade=.65+.35*abs(dot(normalize(N),normalize(float3(-.5,-.3,1))));return Image.rgb*Tint.rgb*shade;'
  assert lib.connect_material_property(sample,'A',unreal.MaterialProperty.MP_OPACITY_MASK)
 inputs=[]
 for key in bindings:
  value=unreal.CustomInput();value.set_editor_property('input_name',key);inputs.append(value)
 code.set_editor_property('inputs',inputs);code.set_editor_property('code',source)
 for key,(node,pin) in bindings.items():assert lib.connect_material_expressions(node,pin,code,key)
 assert lib.connect_material_property(code,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
def graphic(name,source,masked=False):
 mat=unreal.load_asset(folder+'/M_EE_'+name) or assets.create_asset('M_EE_'+name,folder,unreal.Material,unreal.MaterialFactoryNew())
 lib.delete_all_material_expressions(mat)
 mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);mat.set_editor_property('two_sided',True);mat.set_editor_property('used_with_instanced_static_meshes',True)
 mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE)
 if masked:mat.set_editor_property('opacity_mask_clip_value',.15)
 custom=lib.create_material_expression(mat,unreal.MaterialExpressionCustom);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 bindings={'UV':(lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate),''),'N':(lib.create_material_expression(mat,unreal.MaterialExpressionPixelNormalWS),'')}
 if name=='PaperCaveSurface':
  bindings['P']=(lib.create_material_expression(mat,unreal.MaterialExpressionWorldPosition),'XYZ')
  bindings['Cam']=(lib.create_material_expression(mat,unreal.MaterialExpressionCameraPositionWS),'')
 for key,value in [('Tint',unreal.LinearColor(.1,.03,.4,1)),('Accent',unreal.LinearColor(1,.3,.02,1))]:
  node=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);node.set_editor_property('parameter_name',key);node.set_editor_property('default_value',value);bindings[key]=(node,'')
 for key,value in [('WhiteBlend',0.),('Seed',0.),('Sunburst',0.),('DotsReveal',0.),('ApertureRadius',35.)]:
  node=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter);node.set_editor_property('parameter_name',key);node.set_editor_property('default_value',value);bindings[key]=(node,'')
 if masked:
  node=lib.create_material_expression(mat,unreal.MaterialExpressionTextureObjectParameter);node.set_editor_property('parameter_name','Doodle');node.set_editor_property('texture',doodles[0]);bindings['Doodle']=(node,'')
 inputs=[]
 for key in bindings:
  item=unreal.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
 custom.set_editor_property('inputs',inputs);custom.set_editor_property('code',source)
 for key,(node,pin) in bindings.items():assert lib.connect_material_expressions(node,pin,custom,key)
 assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 if masked:
  alpha=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
  for channel in ['r','g','b']:alpha.set_editor_property(channel,False)
  alpha.set_editor_property('a',True);assert lib.connect_material_expressions(custom,'',alpha,'');assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY_MASK)
 lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

graphic('PaperFloor',r'''
float2 p=(UV-.5)*120000;
float2 grid=p/180;
grid.x+=fmod(floor(grid.y),2)*.5;
float aa=max(length(fwidth(grid)),.001);
float dots=1-smoothstep(.24-aa,.24+aa,length(frac(grid)-.5));
dots*=1-smoothstep(.3,1.1,aa);
float3 color=lerp(float3(2.4,2.4,2.4),float3(.003,.003,.003),dots*saturate(DotsReveal));
return float4(color,step(length(p),ApertureRadius));
''',True)
graphic('PaperHorizon',r'''
float2 p=float2(frac(UV.x*2+.5)-.5,(1-UV.y)*6.8+.001);
float angle=atan2(p.y,p.x);
float rays=smoothstep(-.25,.25,sin(angle*120));
float3 color=lerp(float3(.95,.13,.012),float3(1.6,.78,.035),rays);
color=lerp(color,float3(2.1,1.1,.1),exp(-dot(p,p)*14)*.65);
return float4(color,1);
''')

graphic('PaperMineral',r'''
float light=saturate(dot(normalize(N),normalize(float3(.5,-.4,1))));
float shade=.58+.42*light;
return float4(Tint.rgb*shade,1);
''')

graphic('PaperCaveSurface',r'''
float visible=1;
// Before crossing the paper, the cave is seen only THROUGH its physical hole.
if(Cam.x<2217150){
 float t=(2217150-Cam.x)/max(P.x-Cam.x,.01);
 float2 opening=lerp(Cam.yz,P.yz,t)-float2(0,700);
 visible=step(length(opening),440)*step(2217150,P.x);
}
float light=saturate(dot(normalize(N),normalize(float3(.5,.65,1))));
float shade=.48+.52*light;
float layers=.5+.5*sin(P.z*.055+sin(P.x*.012)*.7);
float strata=1-.24*Seed*smoothstep(.65,.9,layers);
return float4(Tint.rgb*shade*strata,visible);
''',True)

graphic('PaperComic',r'''
float light=abs(dot(normalize(N),normalize(float3(-.4,-.3,1))));
float shade=light<.28?.45:light<.7?.72:1;
float edge=min(min(UV.x,1-UV.x),min(UV.y,1-UV.y));
float aa=max(fwidth(UV.x),fwidth(UV.y));
float border=smoothstep(.006-aa,.006+aa,edge);
float2 dotUV=frac(UV*75)-.5;float dotAA=max(fwidth(dotUV.x),fwidth(dotUV.y));
float dots=1-smoothstep(.14-dotAA,.14+dotAA,length(dotUV));
float3 color=Tint.rgb*shade*(1-.17*dots);
if(Sunburst>.5){float2 v=UV-float2(.5,.46);float angle=atan2(v.y,v.x);float ray=smoothstep(.28,.45,sin(angle*56));color=lerp(float3(.92,.10,.035),float3(1,.68,.035),ray);color=lerp(color,float3(1,.83,.3),exp(-dot(v,v)*20)*.5);color*=1-.22*dots;}
color=lerp(float3(.008,.002,.025),color,border);
return float4(lerp(color,float3(10,10,10),saturate(WhiteBlend)),1);
''')

graphic('PaperCutout',r'''
float4 image=Texture2DSample(Doodle,DoodleSampler,UV);
float inner=image.a,outer=image.a;
[unroll]for(int j=0;j<8;++j){float a=j*.785398;float2 direction=float2(cos(a),sin(a));inner=max(inner,Texture2DSample(Doodle,DoodleSampler,UV+direction*.006).a);outer=max(outer,Texture2DSample(Doodle,DoodleSampler,UV+direction*.011).a);}
float swath=smoothstep(.3,.45,frac(UV.y*.9+UV.x*.35+Seed));
float3 paint=lerp(Tint.rgb,Accent.rgb,swath);
float light=abs(dot(normalize(N),normalize(float3(-.6,-.2,1))));paint*=light<.45?.72:1;
float2 dotUV=frac(UV*110)-.5;float dotAA=max(fwidth(dotUV.x),fwidth(dotUV.y));
float dots=1-smoothstep(.14-dotAA,.14+dotAA,length(dotUV));paint*=1-.12*dots;
float black=1-smoothstep(.06,.9,image.r);
float3 color=lerp(paint,float3(.008,.002,.022),black);
color=lerp(float3(.96,.93,.82),color,saturate(image.a*5));
color=lerp(float3(.008,.002,.022),color,saturate(inner*5));
return float4(lerp(color,float3(10,10,10),saturate(WhiteBlend)),outer);
''',True)

graphic('PaperShadow',r'''
float alpha=Texture2DSample(Doodle,DoodleSampler,UV).a;
return float4(lerp(float3(.008,.002,.026),float3(10,10,10),saturate(WhiteBlend)),alpha);
''',True)
graphic('PaperHeroDoodle',r'''
float4 image=Texture2DSample(Doodle,DoodleSampler,UV);
float ink=smoothstep(.06,.80,image.r);
return float4(lerp(float3(.004,.004,.008),float3(1.15,1.15,1.15),ink),image.a);
''',True)
unreal.log('ENDING_PAPER_FINALE_READY')
