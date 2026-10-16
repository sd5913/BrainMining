"""Ending-only cortex, glass neurons, living membranes and illustrated space.
Run in a clean dedicated editor. Rebuilds the existing single demo map.
"""
import unreal, shutil
from pathlib import Path
from datetime import datetime

project=Path(unreal.Paths.project_dir()).resolve()
root='/Game/EndingEmergence'
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
    raise RuntimeError('Use a clean editor; preserve unsaved work.')
backup=project/'Saved/Backups'/('EndingOrganic-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True,exist_ok=True)
shutil.copy2(project/'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap',backup)

def texture(name,source):
    path=root+'/Space/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=root+'/Space';task.destination_name=name
        task.automated=True;task.save=True
        assets.import_asset_tasks([task]);assert task.imported_object_paths,source
    t=unreal.load_asset(path);t.set_editor_property('max_texture_size',2048)
    unreal.EditorAssetLibrary.save_loaded_asset(t)
    return t

stars=texture('T_WestbeamStars',project/'SourceAssets/EndingEmergence/ThirdParty/Westbeam/skybox/1.png')
nebula=texture('T_ParallaxNebula',project/'SourceAssets/EndingEmergence/ThirdParty/StarlightFree/nebula2.png')

def material(name,code,opacity=None,additive=False,textures=None,wpo=None):
    path=root+'/Materials/M_EE_'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_'+name,root+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT if opacity else unreal.BlendMode.BLEND_OPAQUE)
    lib.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    bindings={}
    for key,cls,out in [('P',unreal.MaterialExpressionWorldPosition,'XYZ'),('N',unreal.MaterialExpressionPixelNormalWS,''),('V',unreal.MaterialExpressionCameraVectorWS,''),('UV',unreal.MaterialExpressionTextureCoordinate,''),('T',unreal.MaterialExpressionTime,''),('C',unreal.MaterialExpressionVertexColor,''),('Cam',unreal.MaterialExpressionCameraPositionWS,'')]:
        bindings[key]=(lib.create_material_expression(m,cls),out)
    tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(.13,.30,.45,1))
    bindings['Tint']=(tint,'')
    for key,t in (textures or {}).items():
        n=lib.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('texture',t)
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if t.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        bindings[key]=(n,'')
    def custom(source,output_type,property_,nodes):
        n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);n.set_editor_property('output_type',output_type);n.set_editor_property('code',source)
        inputs=[]
        for key in nodes:
            i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
        n.set_editor_property('inputs',inputs)
        for key,(src,out) in nodes.items():assert lib.connect_material_expressions(src,out,n,key),(name,key,out)
        assert lib.connect_material_property(n,'',property_)
    custom(code,unreal.CustomMaterialOutputType.CMOT_FLOAT3,unreal.MaterialProperty.MP_EMISSIVE_COLOR,bindings)
    if opacity:custom(opacity,unreal.CustomMaterialOutputType.CMOT_FLOAT1,unreal.MaterialProperty.MP_OPACITY,bindings)
    if wpo:
        vertex={key:bindings[key] for key in ['UV','T']}
        vertex['VN']=(lib.create_material_expression(m,unreal.MaterialExpressionVertexNormalWS),'')
        scale=lib.create_material_expression(m,unreal.MaterialExpressionMaterialFunctionCall)
        scale.set_editor_property('material_function',unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/WorldPositionOffset/ObjectScale'))
        vertex['Scale']=(scale,'')
        custom(wpo,unreal.CustomMaterialOutputType.CMOT_FLOAT3,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET,vertex)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m

material('Cortex',r'''
float shade=.30+.70*saturate(dot(normalize(N),normalize(float3(-.5,-.35,1))));
float3 pigment=lerp(float3(.018,.018,.052),float3(.17,.12,.32),smoothstep(.20,.95,C.r));
float rim=pow(1-abs(dot(normalize(N),normalize(V))),3);
return pigment*shade+float3(.025,.045,.09)*rim;
''')
material('NeuronGlass',r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.2);
float light=pow(saturate(dot(normalize(N),normalize(float3(-.4,-.3,1)))),9);
return Tint.rgb*(.32+rim*.9)+float3(.14,.23,.28)*light;
''',opacity=r'''
float nearFade=smoothstep(130,270,length(P-Cam));
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.2);
return (.14+rim*.56)*nearFade;
''')
material('LivingMembrane',r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float flow=.5+.5*sin(UV.x*33+sin(UV.y*27+T*.25)*1.5);
float sheen=pow(saturate(dot(normalize(N),normalize(float3(-.6,-.2,1)))),12);
return lerp(Tint.rgb,float3(.32,.20,.42),flow*.32)*(.16+rim*.85)+float3(.20,.26,.34)*sheen*.45;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),3);
return .018+rim*.42;
''',wpo=r'''
float a=UV.x*6.283185,b=UV.y*3.141593;
float wave=sin(a*3+T*.23)*sin(b*4+T*.17)+.35*sin(a*7-b*5-T*.13);
return VN*dot(abs(Scale),float3(.3333,.3333,.3333))*.8*wave*sin(b);
''')
material('SpaceField',r'''
float3 s=Texture2DSample(StarMap,StarMapSampler,UV).rgb;
float edge=smoothstep(0,.13,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));
return s*float3(.72,.82,1.2)*3*edge;
''',additive=True,textures={'StarMap':stars})
material('NebulaField',r'''
float3 c=Texture2DSample(NebulaMap,NebulaMapSampler,UV).rgb;
float edge=smoothstep(0,.12,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));
return c*float3(.68,.83,1.05)*edge*.85;
''',additive=True,textures={'NebulaMap':nebula})
base='/Game/Planet_Project/Assets/Textures/Earth/'
# Read the acquired textures, but remove photographic surface detail. Their
# geography becomes broad blue shapes under the same amber city-light palette.
planet_textures={key:unreal.load_asset(base+name) for key,name in {'DayMap':'T_8k_earth_daymap','NightMap':'T_8k_earth_nightmap','CloudMap':'T_8k_earth_clouds'}.items()}
assert all(planet_textures.values())
material('Planet',r'''
float3 n=normalize(N);
float2 q=float2(frac(-atan2(n.y,n.x)/6.2831853+.35),acos(clamp(n.z,-1,1))/3.1415927);
float3 day=Texture2DSampleLevel(DayMap,DayMapSampler,q,5).rgb;
float3 night=Texture2DSampleLevel(NightMap,NightMapSampler,q,1.8).rgb;
float cloud=Texture2DSampleLevel(CloudMap,CloudMapSampler,q+float2(T*.00010,0),5).r;
float land=smoothstep(.008,.065,day.r+day.g*.45-day.b*.80);
float lit=smoothstep(-.5,.8,dot(n,normalize(float3(-.25,-.5,1))));
float3 ground=lerp(float3(.003,.012,.030),float3(.016,.031,.055),land)*(.6+lit*.4);
float towns=pow(saturate(dot(night,float3(.5,.35,.15))*3.5),.8);
float3 warm=float3(1,.49,.15)*towns*2.2;
float c=smoothstep(.23,.63,cloud)*.32;
float rim=pow(1-saturate(dot(n,normalize(V))),4);
return (ground+warm)*(1-c*.55)+float3(.06,.10,.16)*c+float3(.018,.065,.15)*rim;
''',textures=planet_textures)

assert levels.load_level(root+'/Maps/Lvl_EndingEmergence_Demo')
demo=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
demo.build_demo()
assert levels.save_current_level()
unreal.log('ENDING_ORGANIC_ART_READY: compact cell, cortex, translucent neurons, night Earth, layered stars, living membranes')
