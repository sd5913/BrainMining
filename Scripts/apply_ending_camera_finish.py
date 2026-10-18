"""Rebuild the ending's pulse lens and comic exit; preserve shared game materials."""
import unreal

finish_lib=unreal.MaterialEditingLibrary
finish_assets=unreal.AssetToolsHelpers.get_asset_tools()
finish_folder='/Game/EndingEmergence/Materials'

def post_finish(name,code,priority,bridge=False):
    path=finish_folder+'/M_EE_'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else finish_assets.create_asset('M_EE_'+name,finish_folder,unreal.Material,unreal.MaterialFactoryNew())
    finish_lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    m.set_editor_property('blendable_priority',priority)
    custom=finish_lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    bindings={}
    uv=finish_lib.create_material_expression(m,unreal.MaterialExpressionScreenPosition);bindings['UV']=(uv,'ViewportUV')
    scene=finish_lib.create_material_expression(m,unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0);bindings['SC']=(scene,'Color')
    param=finish_lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    param.set_editor_property('parameter_name','Strength');param.set_editor_property('default_value',0.);bindings['Strength']=(param,'')
    bindings['T']=(finish_lib.create_material_expression(m,unreal.MaterialExpressionTime),'')
    if bridge:
        for name in ['Bridge','Travel','Handoff','Rise']:
            value=finish_lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
            value.set_editor_property('parameter_name',name);value.set_editor_property('default_value',0.)
            bindings[name]=(value,'')
        tex=finish_lib.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter)
        tex.set_editor_property('parameter_name','PreviousFrame')
        tex.set_editor_property('texture',unreal.load_asset('/Game/EndingEmergence/Planet/T_NASA_NightGlobal'))
        tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        bindings['PreviousFrame']=(tex,'')
    inputs=[]
    for key in bindings:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',key);inputs.append(inp)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code','#if PLATFORM_ANDROID\nreturn SC.rgb;\n#else\n'+code+'\n#endif')
    for key,(node,output) in bindings.items():assert finish_lib.connect_material_expressions(node,output,custom,key)
    assert finish_lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish_lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)

post_finish('ScaleZoom',r'''
float amount=saturate(Strength), warp=clamp(Strength,0,2.5), blend=saturate(Bridge), travel=saturate(Travel);
float handoff=saturate(Handoff),rise=saturate(Rise);
[branch] if(amount<.001 && blend<.001) return SC.rgb;
float2 p=UV-.5;
float edge=smoothstep(.015,.3,dot(p,p));
// World-space camera motion supplies the lift and perspective change.
// Only radial shutter trails anticipate and continue the spatial camera rush.
// A short outgoing exposure overlaps the live arrival. Both images preserve
// their aspect ratio; only uniform radial scaling is allowed.
[branch] if(handoff>.0001 || rise>.0001) {
    float3 moving=0,oldExposure=0;
    float rush=max(handoff,rise);
    [unroll] for(int j=0;j<11;j++) {
        float tap=j/10.0;
        float2 q=.5+p*(1+tap*.95*rush);
        moving+=SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(q,.002,.998),14),14,false).rgb;
        [branch] if(blend>.001) {
            float2 oldUV=.5+p*(1+travel*.32+tap*.60);
            oldExposure+=Texture2DSample(PreviousFrame,PreviousFrameSampler,clamp(oldUV,.002,.998)).rgb;
        }
    }
    float angle=atan2(p.y,p.x),radius=length(p);
    float streak=pow(saturate(.5+.5*sin(angle*139+sin(angle*11)*2)),32)*smoothstep(.24,.65,radius);
    return lerp(moving/11,oldExposure/11,blend)*(1+streak*rush*.18);
}
float3 current=0,previous=0;
[unroll] for(int i=0;i<9;i++) {
    float tap=i/8.0;
    float2 q=.5+p*(1+tap*.55*warp);
    float3 sampleColour=SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(q,.002,.998),14),14,false).rgb;
    current+=sampleColour;
    [branch] if(blend>.001) {
        // Preserve the earlier brain bridge without anisotropic translation.
        float2 oldUV=.5+p*(1+travel*3.8+tap*.48*warp);
        float valid=1-smoothstep(.45,.66,length(oldUV-.5));
        float3 old=Texture2DSample(PreviousFrame,PreviousFrameSampler,saturate(oldUV)).rgb;
        previous+=lerp(sampleColour,old,valid);
    }
}
current=lerp(SC.rgb,current/9,amount*(.25+edge*.75));
float3 result=lerp(current,previous/9,blend);
// Dim radial trails preserve the original scene colours, never a white overlay.
float a=atan2(p.y,p.x),r=length(p);
float streak=pow(saturate(.5+.5*sin(a*139+sin(a*11)*2)),32)*smoothstep(.24,.65,r);
return result*(1+streak*amount*.14);
''',35,bridge=True)

post_finish('PulseTransit',r'''
float strength=saturate(Strength);
float2 p=UV-.5;float r=dot(p,p),edge=smoothstep(.018,.25,r);
// Lens-driven transit: strong bulge, slight tangential shear and radial defocus.
// No drawn speed lines; motion comes from distortion of the scene itself.
float2 warped=.5+p*(1+strength*(2.5*r-.19));
warped+=float2(-p.y,p.x)*strength*.09*edge*sin(T*3.1);
float3 colour=0;
[unroll] for(int i=0;i<13;i++) {
    float tap=(i-6)/6.0;
    float2 q=warped+p*tap*.52*strength*(.15+edge*.85);
    colour+=SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(q,.002,.998),14),14,false).rgb;
}
colour/=13;
float2 chroma=p*.022*strength*edge;
colour.r=lerp(colour.r,SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(warped+chroma,.002,.998),14),14,false).r,.4*strength);
colour.b=lerp(colour.b,SceneTextureLookup(ViewportUVToSceneTextureUV(clamp(warped-chroma,.002,.998),14),14,false).b,.4*strength);
return lerp(SC.rgb,colour,strength)*(1-edge*strength*.24);
''',40)

post_finish('ExitInk',r'''
float2 p=(UV-.5)*float2(1.77778,1);float r=length(p),a=atan2(p.y,p.x);
float strength=saturate(Strength);
float sector=floor((a+3.141593)*34);
float seed=frac(sin(sector*13.71)*4375.23);
float stroke=pow(saturate(.5+.5*cos(a*214+sin(a*17)*2)),18);
stroke*=smoothstep(.24,.68,r)*smoothstep(.05,.35,frac(r*1.6-T*2.5+seed));
float3 paper=lerp(float3(.012,.015,.02),float3(.95,.95,.91),step(.53,seed));
float3 scene=lerp(SC.rgb,dot(SC.rgb,float3(.21,.72,.07)).xxx,strength*.65);
return lerp(scene,paper,stroke*strength*.88);
''',50)

def fade_mask(material):
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
    fade=finish_lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
    fade.set_editor_property('parameter_name','Fade');fade.set_editor_property('default_value',1.)
    pixel=finish_lib.create_material_expression(material,unreal.MaterialExpressionScreenPosition)
    mask=finish_lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs=[]
    for name in ['Fade','Pixel']:
        inp=unreal.CustomInput();inp.set_editor_property('input_name',name);inputs.append(inp)
    mask.set_editor_property('inputs',inputs)
    mask.set_editor_property('code','float noise=frac(sin(dot(floor(Pixel),float2(12.9898,78.233)))*43758.5453); return saturate(Fade)*1.002-noise;')
    finish_lib.connect_material_expressions(fade,'',mask,'Fade')
    finish_lib.connect_material_expressions(pixel,'PixelPosition',mask,'Pixel')
    finish_lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    # The custom expression returns Fade-noise; the cut threshold must be zero.
    material.set_editor_property('opacity_mask_clip_value',.001)
    finish_lib.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material)

for finish_name in ['ComicRock','ComicDrill']:
    fade_mask(unreal.load_asset(finish_folder+'/M_EE_'+finish_name))

stroke_path=finish_folder+'/M_EE_ExitStroke'
stroke=unreal.load_asset(stroke_path) if unreal.EditorAssetLibrary.does_asset_exist(stroke_path) else finish_assets.create_asset('M_EE_ExitStroke',finish_folder,unreal.Material,unreal.MaterialFactoryNew())
finish_lib.delete_all_material_expressions(stroke)
stroke.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
stroke.set_editor_property('used_with_instanced_static_meshes',True)
tint=finish_lib.create_material_expression(stroke,unreal.MaterialExpressionVectorParameter)
tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
finish_lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
fade_mask(stroke)
unreal.log('ENDING_CAMERA_FINISH_READY: pulse lens, tool fade, monochrome exit streaks')
