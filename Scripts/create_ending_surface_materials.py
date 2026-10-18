"""Ending-only photographic surfaces. Can also run without rebuilding the map."""
import unreal
from pathlib import Path

ROOT='/Game/EndingEmergence'
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
source=Path(unreal.Paths.project_dir())/'SourceAssets/EndingEmergence/ThirdParty'

def texture(name, filename, linear=False):
    path=ROOT+'/Textures/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask()
        task.filename=str(source/filename);task.destination_path=ROOT+'/Textures';task.destination_name=name
        task.automated=True;task.save=True;task.replace_existing=False
        assets.import_asset_tasks([task])
    tex=unreal.load_asset(path)
    assert tex,filename
    tex.set_editor_property('srgb',not linear)
    if linear: tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex

paper=texture('T_EE_PaperFiber','Paper001/Paper001_1K-PNG_Color.png')
normal=texture('T_EE_PaperNormal','Paper001/Paper001_1K-PNG_NormalDX.png',True)
rough=texture('T_EE_PaperRoughness','Paper001/Paper001_1K-PNG_Roughness.png',True)
splash=texture('T_EE_WaterPhotograph','WaterSplash_ClickerHappy.jpg')
drops=texture('T_EE_DropletsPhotograph','WaterDroplets_Bicanski.jpg')
ink=texture('T_EE_HandwrittenInk','EndingHandwrittenInk.png',True)
ideas=[]
for i in range(1,11):
    photo=texture(f'T_EE_Idea{i}',Path(unreal.Paths.project_dir()).resolve()/'Art'/'Ending'/f'idea{i}.jpg')
    photo.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    photo.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(photo)
    ideas.append(photo)

def make(name,code,textures,scalars={},translucent=False,post=False,vectors={}):
    path=ROOT+'/Materials/M_EE_'+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_'+name,ROOT+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',True)
    if post:
        mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
        mat.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    else:
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_OPAQUE)
    def node(cls):return lib.create_material_expression(mat,cls)
    custom=node(unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    bindings=[]
    uv=node(unreal.MaterialExpressionScreenPosition if post else unreal.MaterialExpressionTextureCoordinate)
    bindings.append(('UV',uv,'ViewportUV' if post else ''))
    time=node(unreal.MaterialExpressionTime);bindings.append(('T',time,''))
    if post:
        scene=node(unreal.MaterialExpressionSceneTexture);scene.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
        bindings.append(('SC',scene,'Color'))
    else:
        norm=node(unreal.MaterialExpressionPixelNormalWS);view=node(unreal.MaterialExpressionCameraVectorWS)
        bindings.extend([('N',norm,''),('V',view,'')])
        pos=node(unreal.MaterialExpressionWorldPosition);center=node(unreal.MaterialExpressionObjectPositionWS)
        bindings.extend([('P',pos,'XYZ'),('Center',center,'')])
        tint=node(unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
        bindings.append(('Tint',tint,''))
    for key,tex in textures.items():
        n=node(unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('texture',tex)
        # A raw normal texture is sampled as data for the camera-facing lighting.
        if not tex.get_editor_property('srgb'):n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        bindings.append((key,n,''))
    for key,value in scalars.items():
        n=node(unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',value)
        bindings.append((key,n,''))
    for key,value in vectors.items():
        n=node(unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',unreal.LinearColor(*value))
        bindings.append((key,n,'RGBA'))
    inputs=[]
    for key,_,_ in bindings:
        i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code',('#if PLATFORM_ANDROID\nreturn float4(SC.rgb,1);\n#else\n'+code+'\n#endif') if post else code)
    for key,n,output in bindings:assert lib.connect_material_expressions(n,output,custom,key),key
    rgb=node(unreal.MaterialExpressionComponentMask);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True)
    assert lib.connect_material_expressions(custom,'',rgb,'');lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent:
        alpha=node(unreal.MaterialExpressionComponentMask);alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True)
        assert lib.connect_material_expressions(custom,'',alpha,'');lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
        refraction=node(unreal.MaterialExpressionConstant);refraction.set_editor_property('r',1.035)
        lib.connect_material_property(refraction,'',unreal.MaterialProperty.MP_REFRACTION)
    lib.recompile_material(mat);assert unreal.EditorAssetLibrary.save_loaded_asset(mat)

make('IdeaPaper',r'''
float2 q=UV+Seed;
// Sampling the portrait original this way rotates every photograph 90 degrees left.
float2 crop=PhotoCrop.xy+UV*PhotoCrop.zw;
float2 sourceUV=float2(1-crop.y,crop.x);
float3 p=Texture2DSample(Idea,IdeaSampler,sourceUV).rgb;
float3 n=Texture2DSample(FiberNormal,FiberNormalSampler,q).rgb*2-1;
float r=Texture2DSample(Rough,RoughSampler,q).r;
float fold=abs(UV.x-.51)+.13*sin(UV.y*7+Seed*21);
float crease=exp(-abs(fold-.13)*170);
float damp=pow(saturate(sin(UV.x*8+Seed*31)*cos(UV.y*11+Seed*17)),3);
float shade=.82+.18*saturate(dot(normalize(float3(n.xy*.8,n.z)),normalize(float3(-.5,.7,1))));
shade*=.84+.16*abs(dot(normalize(N),normalize(V)));
float edge=min(min(UV.x,1-UV.x),min(UV.y,1-UV.y));
shade*=.91+.09*smoothstep(0,.018,edge);
float3 color=p*Tint.rgb*shade*(1-.08*damp-.035*crease);
return float4(color+(1-r)*.008,1);
''',{'Idea':ideas[0],'FiberNormal':normal,'Rough':rough},{'Seed':0.},vectors={'PhotoCrop':(0.,0.,1.,1.)})

make('ApertureVoid','return float4(0,0,0,1);',{})

make('WaterJet',r'''
float2 q=float2(.43+UV.x*.15,.39+frac(UV.y*1.7-T*1.8+Seed)*.27);
float3 photo=Texture2DSample(Water,WaterSampler,q).rgb;
float lum=dot(photo,float3(.3,.59,.11));
float fres=pow(1-abs(dot(normalize(N),normalize(V))),2);
float rim=smoothstep(0,.10,UV.x)*smoothstep(0,.10,1-UV.x);
float torn=smoothstep(.025,.16,lum+.035*sin(UV.y*77+UV.x*45-T*35));
float alpha=saturate((.08+lum*.9+fres*.19)*rim*torn*Flow);
float3 c=lerp(float3(.09,.13,.15),float3(.52,.59,.62),saturate(lum*1.9+fres*.30));
return float4(c,alpha);
''',{'Water':splash},{'Seed':0.,'Flow':1.},True)

make('WaterSpray',r'''
float fres=pow(1-abs(dot(normalize(N),normalize(V))),3);
float glint=pow(saturate(dot(normalize(N),normalize(float3(-.6,-.4,1)))),40);
float photo=Texture2DSample(Water,WaterSampler,float2(.43+UV.x*.15,.4+UV.y*.22)).r;
return float4(float3(.08,.10,.11)+float3(.55,.61,.65)*(fres*.6+glint+photo*.5),saturate(.18+fres*.65+glint));
''',{'Water':splash},translucent=True)

make('WetLens',r'''
// The photograph supplies irregular real droplet silhouettes and highlight detail.
// Sample its green excess to reject the underlying grey surface and red grid.
float2 p=float2(UV.x*.72+.12,UV.y*.58+.24-SplashTime*.005);
float3 d=Texture2DSample(Drops,DropsSampler,p).rgb;
float h=saturate((d.g-d.r)*4);
float3 dx=Texture2DSample(Drops,DropsSampler,p+float2(.0015,0)).rgb;
float3 dy=Texture2DSample(Drops,DropsSampler,p+float2(0,.0015)).rgb;
float2 grad=float2((dx.g-dx.r)-(d.g-d.r),(dy.g-dy.r)-(d.g-d.r));
float impact=saturate(1-abs(SplashTime-.48)/.4);
float2 wave=float2(sin(UV.y*35+SplashTime*8),cos(UV.x*28-SplashTime*7));
float2 offset=Wet*(clamp(grad,-.1,.1)*.018+wave*(.001+.009*impact));
float3 color=SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(UV+offset,.001,.999),14),14,false).rgb;
float shine=saturate(dot(d,float3(.3,.6,.1))-.6)*h;
return float4(color*(1-Wet*h*.06)+Wet*(shine*.20+abs(grad.x-grad.y)*.15)+SC.rgb*.00000001,1);
''',{'Drops':drops},{'Wet':0.,'SplashTime':100.},post=True)

# Rebuilt with the other ending surfaces so old saved materials cannot retain
# the previous flash contribution or lose the third hit's continuing ripple.
make('Exhaustion',r'''
float2 p=UV-.5;
float aspect=View.ViewSizeAndInvSize.x/View.ViewSizeAndInvSize.y;
float radius=length(p*float2(aspect,1));
float wave=0;
if(RippleAge>=0 && RippleAge<1.2)
    wave=exp(-pow((radius-RippleAge*1.1)/.027,2))*exp(-RippleAge*1.6);
float kick=RippleAge>=0 && RippleAge<.65 ? exp(-RippleAge*7)*(1-smoothstep(.48,.65,RippleAge)) : 0;
float2 shake=float2(sin(RippleAge*71)+.3*sin(RippleAge*129),sin(RippleAge*93+.8))*ShakeStrength*kick;
float2 warped=clamp(UV+normalize(p+float2(.00001,.00001))*.014*wave+shake,.002,.998);
float3 rgb=SceneTextureLookup(ViewportUVToSceneTextureUV(warped,14),14,false).rgb;
float luma=dot(rgb,float3(.2126,.7152,.0722));
// Refraction supplies the wave: no additive white pulse across the screen.
return float4(lerp(rgb,luma.xxx,saturate(Gray))*(1-Gray*.16)+SC.rgb*.00000001,1);
''',{}, {'Gray':0.,'RippleAge':100.,'ShakeStrength':0.},post=True)

make('CosmicDebris',r'''
float3 q=(P-Center)*.7;
float3 n=abs(normalize(N));n/=max(.001,n.x+n.y+n.z);
float stars=0;
for(int axis=0;axis<3;axis++) {
    float2 uv=axis==0?q.yz:axis==1?q.xz:q.xy;
    float2 cell=floor(uv),d=frac(uv)-.5;
    float hash=frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453);
    float starDot=exp(-dot(d,d)*180)*step(.76,hash);
    stars+=starDot*(axis==0?n.x:axis==1?n.y:n.z);
}
float cloud=.5+.5*sin(q.x*.27+sin(q.z*.31)*2)*cos(q.y*.24-q.z*.19);
float3 nebula=lerp(float3(.008,.012,.035),float3(.035,.012,.075),cloud);
nebula+=float3(.006,.028,.036)*pow(cloud,3);
return float4(nebula+float3(.78,.91,1.0)*stars*1.8,1);
''',{})
