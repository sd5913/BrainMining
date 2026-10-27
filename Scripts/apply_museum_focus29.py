"""Author stable museum focus outlines and animate the painted motif itself."""
import json
from pathlib import Path
import unreal

ml=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
out=Path(unreal.Paths.project_saved_dir())/'Validation/MuseumFocus29'
out.mkdir(parents=True,exist_ok=True)

def node(m,cls,**props):
    n=ml.create_material_expression(m,cls)
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def custom(m,code,bindings):
    n=node(m,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,code=code)
    inputs=[]
    for name in bindings:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    n.set_editor_property('inputs',inputs)
    for name,(source,pin) in bindings.items():assert ml.connect_material_expressions(source,pin,n,name)
    assert ml.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return n

path='/Game/MainGame/Materials/M_MuseumFocusOutline'
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_MuseumFocusOutline','/Game/MainGame/Materials',unreal.Material,unreal.MaterialFactoryNew())
ml.delete_all_material_expressions(m)
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
bindings={name:(node(m,unreal.MaterialExpressionSceneTexture,scene_texture_id=tex),'Color') for name,tex in [('Base',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),('Depth',unreal.SceneTextureId.PPI_SCENE_DEPTH),('Mask',unreal.SceneTextureId.PPI_CUSTOM_DEPTH),('Stencil',unreal.SceneTextureId.PPI_CUSTOM_STENCIL)]}
custom(m,r'''
#if SHADING_PATH_MOBILE
return Base.rgb;
#else
float2 viewPixel=GetViewportUV(Parameters)*View.ViewSizeAndInvSize.xy;
if(any(viewPixel<5.0)||any(viewPixel>View.ViewSizeAndInvSize.xy-5.0))return Base.rgb;
float2 uv=GetDefaultSceneTextureUV(Parameters,PPI_CustomDepth);
float2 texel=View.BufferSizeAndInvSize.zw;
float center=Stencil.r;
float idle=0,focus=0,outer=0;
[unroll]for(int band=0;band<3;++band){
 float width=band==0?1.0:band==1?2.7:3.8;
 [unroll]for(int y=-1;y<=1;++y){[unroll]for(int x=-1;x<=1;++x){
  float2 p=uv+float2(x,y)*texel*width;
  float id=SceneTextureLookup(p,PPI_CustomStencil,false).r;
  float cd=SceneTextureLookup(p,PPI_CustomDepth,false).r;
  float sd=SceneTextureLookup(p,PPI_SceneDepth,false).r;
  float visible=(cd>0&&cd<10000000&&cd<=sd+1.0&&cd<=Depth.r+2.0&&abs(id-center)>.5)?1:0;
  if(band==0&&id>=20&&id<=40)idle=max(idle,visible);
  if(band==1&&id==250)focus=max(focus,visible);
  if(band==2&&id==250)outer=max(outer,visible);
 }}
}
float3 color=lerp(Base.rgb,float3(.73,.78,.79),idle*.75);
color=lerp(color,float3(.055,.045,.035),outer*.80);
return lerp(color,float3(1.0,.95,.85),focus);
#endif
''',bindings)
ml.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)

painting=unreal.load_asset('/Game/MouseMuseum/M_MouseKiss')
assert painting
ml.delete_all_material_expressions(painting)
painting.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
painting.set_editor_property('two_sided',True)
uv=node(painting,unreal.MaterialExpressionTextureCoordinate)
tex=node(painting,unreal.MaterialExpressionTextureObject,texture=unreal.load_asset('/Game/MouseMuseum/T_MouseKiss'))
clock=node(painting,unreal.MaterialExpressionTime)
highlight=node(painting,unreal.MaterialExpressionScalarParameter,parameter_name='Highlight',default_value=0.)
custom(painting,r'''
float2 size=float2(126,168),center=float2(.6374,.5656);
float2 local=(UV-center)*size;
float r=length(local),a=atan2(local.y,local.x);
float inside=1-smoothstep(2.35,2.9,r);
// Rotate and ripple the original colored paint inside the motif; its position
// and the surrounding canvas remain fixed. No idle UI ring or white glow.
float twist=inside*(Time*.38+.20*sin(Time*1.2-r*1.7));
float wave=1+.055*inside*sin(a*3-r*2+Time*1.5);
float2 flowing=float2(cos(a+twist),sin(a+twist))*r*wave;
float2 sampleUV=center+lerp(local,flowing,inside)/size;
float3 paint=Texture2DSample(Image,ImageSampler,sampleUV).rgb*.82;
float ring=1-smoothstep(.12,.4,abs(r-2.9));
return paint+ring*Highlight*float3(1,1,1)*2;
''',{'UV':(uv,''),'Image':(tex,''),'Time':(clock,''),'Highlight':(highlight,'')})
ml.recompile_material(painting)
assert unreal.EditorAssetLibrary.save_loaded_asset(painting,False)
(out/'materials.json').write_text(json.dumps({'outline':m.get_path_name(),'painting':painting.get_path_name(),'saved':True},indent=2))
unreal.SystemLibrary.quit_editor()
