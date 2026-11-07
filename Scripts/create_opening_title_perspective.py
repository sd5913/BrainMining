"""Menu logo perspective and a subtle lower-left shadow; no wall material edits."""
import unreal
folder='/Game/StartRoom/UI';name='M_OpeningTitlePerspective';path=folder+'/'+name
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
lib=unreal.MaterialEditingLibrary
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('two_sided',True)
lib.delete_all_material_expressions(m)
def node(cls,x,y,**props):
 n=lib.create_material_expression(m,cls,x,y)
 for k,v in props.items():n.set_editor_property(k,v)
 return n
uv=node(unreal.MaterialExpressionTextureCoordinate,-650,-100)
logo=node(unreal.MaterialExpressionTextureObjectParameter,-650,100,parameter_name='Logo',texture=unreal.load_asset(folder+'/T_OpeningTitle'))
depth=node(unreal.MaterialExpressionScalarParameter,-650,300,parameter_name='PerspectiveDepth',default_value=.28,slider_min=0.,slider_max=.65)
shadow_x=node(unreal.MaterialExpressionScalarParameter,-650,420,parameter_name='ShadowOffsetX',default_value=-.005)
shadow_y=node(unreal.MaterialExpressionScalarParameter,-650,540,parameter_name='ShadowOffsetY',default_value=.008)
shadow_opacity=node(unreal.MaterialExpressionScalarParameter,-650,660,parameter_name='ShadowOpacity',default_value=.6,slider_min=0.,slider_max=1.)
shadow_softness=node(unreal.MaterialExpressionScalarParameter,-650,780,parameter_name='ShadowSoftness',default_value=.0015,slider_min=0.,slider_max=.01)
custom=node(unreal.MaterialExpressionCustom,-280,0,description='Perspective with lower-left button-style shadow',output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
pins=[]
for key in ['UV','Logo','Depth','ShadowX','ShadowY','ShadowOpacity','ShadowSoftness']:
 pin=unreal.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
custom.set_editor_property('inputs',pins)
custom.set_editor_property('code','''
float k = clamp(Depth, 0.0, 0.8);
float right = 0.98;
float2 p;
p.x = UV.x / max(right * (1.0 + k) - k * UV.x, 0.001);
p.y = (UV.y - 0.5) * (1.0 + k * p.x) + 0.5;
float inside = step(0.0,p.x)*step(p.x,1.0)*step(0.0,p.y)*step(p.y,1.0);
float4 ink = Texture2DSample(Logo, LogoSampler, p);
ink.a *= inside;
// Offset in widget UV space before inverse perspective: the whole shadow
// points down and left, away from the light in the centre of the room.
float2 offsets[5] = {float2(0,0),float2(-1,0),float2(1,0),float2(0,-1),float2(0,1)};
float shadow = 0.0;
[unroll]
for (int i = 0; i < 5; ++i)
{
    float2 uv = UV - float2(ShadowX,ShadowY) + offsets[i]*max(ShadowSoftness,0.0);
    float2 q;
    q.x = uv.x / max(right * (1.0 + k) - k * uv.x, 0.001);
    q.y = (uv.y - 0.5) * (1.0 + k * q.x) + 0.5;
    float bounds = step(0.0,q.x)*step(q.x,1.0)*step(0.0,q.y)*step(q.y,1.0);
    shadow += Texture2DSample(Logo, LogoSampler, q).a * bounds * (i == 0 ? 0.4 : 0.15);
}
float behind = saturate(shadow*ShadowOpacity)*(1.0-ink.a);
float alpha = ink.a+behind;
// Match the buttons' warm dark-brown shadow; preserve the logo's own colors.
float3 color = (ink.rgb*ink.a+float3(0.035,0.018,0.012)*behind)/max(alpha,0.0001);
return float4(color,alpha);
''')
for n,key in [(uv,'UV'),(logo,'Logo'),(depth,'Depth'),(shadow_x,'ShadowX'),(shadow_y,'ShadowY'),(shadow_opacity,'ShadowOpacity'),(shadow_softness,'ShadowSoftness')]:assert lib.connect_material_expressions(n,'',custom,key)
rgb=node(unreal.MaterialExpressionComponentMask,10,-80,r=True,g=True,b=True,a=False)
alpha=node(unreal.MaterialExpressionComponentMask,10,110,r=False,g=False,b=False,a=True)
assert lib.connect_material_expressions(custom,'',rgb,'')
assert lib.connect_material_expressions(custom,'',alpha,'')
assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
unreal.SystemLibrary.quit_editor()
