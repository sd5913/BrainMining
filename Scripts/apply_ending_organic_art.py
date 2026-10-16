"""Ending-only cortex, glass neurons, living membranes and illustrated space.
Run in a clean dedicated editor. Rebuilds the existing single demo map.
"""
import unreal, shutil, math, random
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
    if name=='NeuralFilament':m.set_editor_property('used_with_spline_meshes',True)
    bindings={}
    for key,cls,out in [('P',unreal.MaterialExpressionWorldPosition,'XYZ'),('N',unreal.MaterialExpressionPixelNormalWS,''),('V',unreal.MaterialExpressionCameraVectorWS,''),('UV',unreal.MaterialExpressionTextureCoordinate,''),('T',unreal.MaterialExpressionTime,''),('C',unreal.MaterialExpressionVertexColor,''),('Cam',unreal.MaterialExpressionCameraPositionWS,''),('Obj',unreal.MaterialExpressionObjectPositionWS,'')]:
        bindings[key]=(lib.create_material_expression(m,cls),out)
    tint=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(.13,.30,.45,1))
    bindings['Tint']=(tint,'')
    for key,value in [('Reveal',1.),('Variant',0.),('Fade',1.),('Spin',0.),('Activity',0.)]:
        scalar=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
        scalar.set_editor_property('parameter_name',key);scalar.set_editor_property('default_value',value)
        bindings[key]=(scalar,'')
    for key,t in (textures or {}).items():
        n=lib.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('texture',t)
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if t.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if t.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
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

# Closed shared meshes with intentionally split face normals, not smooth spheres.
for name,steps in [('FacetNucleus',4),('FacetOrganelle',3),('FacetGranule',3)]:
    mesh=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_sphere_box(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),radius=50,steps_x=steps,steps_y=steps,steps_z=steps)
    for index in range(4000):
        point,valid=unreal.GeometryScript_MeshQueries.get_vertex_position(mesh,index)
        if not valid:break
        wobble=1+.065*math.sin(point.x*.09+point.z*.04)*math.cos(point.y*.075)
        unreal.GeometryScript_MeshEdits.set_vertex_position(mesh,index,point*wobble,True)
    unreal.GeometryScript_Normals.set_per_face_normals(mesh)
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
    path=root+'/Meshes/SM_EE_'+name
    asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.duplicate_asset('SM_EE_'+name,root+'/Meshes',unreal.load_asset('/Engine/BasicShapes/Sphere'))
    options=unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False,enable_recompute_tangents=False)
    _,status=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh,asset,options,unreal.GeometryScriptMeshWriteLOD())
    assert status==unreal.GeometryScriptOutcomePins.SUCCESS
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
    unreal.log('ENDING_FACETS '+name+' triangles='+str(mesh.get_triangle_count()))

# Ending-only organic soma and tapering neurites. They replace the round beads
# and uniform cylindrical stalks without changing the possession graph.
for name in ['FacetNeuronSoma','FacetNeuronBranch','FacetNeuronArbor','FacetNeuronArbor1','FacetNeuronArbor2','FacetNeuronArbor3']:
    mesh=unreal.DynamicMesh()
    if name=='FacetNeuronSoma':
        unreal.GeometryScript_Primitives.append_sphere_box(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),radius=50,steps_x=10,steps_y=10,steps_z=10)
        for index in range(10000):
            point,valid=unreal.GeometryScript_MeshQueries.get_vertex_position(mesh,index)
            if not valid:break
            length=max(.001,math.sqrt(point.x**2+point.y**2+point.z**2))
            x,y,z=point.x/length,point.y/length,point.z/length
            # No radial petals: an asymmetric, gently angular cell body.
            wobble=.96+.09*math.sin(x*3+y*2-z)+.06*math.sin(y*4-z*2+.8)
            unreal.GeometryScript_MeshEdits.set_vertex_position(mesh,index,point*wobble,True)
    elif name=='FacetNeuronBranch':
        unreal.GeometryScript_Primitives.append_cone(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),base_radius=50,top_radius=22,height=100,radial_steps=12,height_steps=5,capped=True,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    else:
        def tube(points,radius,end_scale):
            polygon=[unreal.Vector2D(radius*math.cos(i*2*math.pi/10),radius*math.sin(i*2*math.pi/10)) for i in range(10)]
            unreal.GeometryScript_Primitives.append_simple_swept_polygon(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),polygon,points,loop=False,capped=True,start_scale=1,end_scale=end_scale)
        def curve(a,b,c,d,count=18):
            return [a*(1-t)**3+b*(3*(1-t)**2*t)+c*(3*(1-t)*t*t)+d*t**3 for t in [i/count for i in range(count+1)]]
        variant=int(name[-1]) if name[-1].isdigit() else 0
        rng=random.Random(831+variant*97)
        count=6+variant
        for branch in range(count):
            angle=branch*2*math.pi/count+rng.uniform(-.22,.22)
            direction=unreal.Vector(math.cos(angle),math.sin(angle),rng.uniform(-.18,.18))
            side=unreal.Vector(-math.sin(angle),math.cos(angle),rng.uniform(-.12,.12))
            reach=rng.uniform(180,320)*(1.35 if branch==0 else 1)
            bend=rng.uniform(-40,40)
            points=curve(direction*34,direction*reach*.33+side*bend,direction*reach*.72-side*bend*.8,direction*reach+side*bend*.5)
            tube(points,rng.uniform(2.6,4.2),.10)
            for fork,index in enumerate([10,14]):
                start=points[index]
                forkdir=direction*.55+side*(-.8 if fork==0 else .85)
                tube(curve(start,start+forkdir*27,start+forkdir*58+direction*12,start+forkdir*rng.uniform(70,130)-side*12,12),1.4,.10)
    unreal.GeometryScript_Normals.set_per_vertex_normals(mesh)
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
    path=root+'/Meshes/SM_EE_'+name
    asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.duplicate_asset('SM_EE_'+name,root+'/Meshes',unreal.load_asset('/Engine/BasicShapes/Sphere'))
    options=unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False,enable_recompute_tangents=False)
    _,status=unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh,asset,options,unreal.GeometryScriptMeshWriteLOD())
    assert status==unreal.GeometryScriptOutcomePins.SUCCESS
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
    unreal.log('ENDING_NEURON_MORPHOLOGY '+name+' triangles='+str(mesh.get_triangle_count()))

material('FacetSurface',r'''
float3 n=normalize(N);
float light=saturate(dot(n,normalize(float3(-.6,-.35,1))));
float shade=.32+.60*light+.18*smoothstep(.7,.9,light);
float edge=pow(1-abs(dot(n,normalize(V))),4);
return Tint.rgb*shade*1.6+float3(.025,.035,.055)*edge;
''')
material('CellSpeck',r'''
return Tint.rgb*2.8*Fade*smoothstep(70000,170000,length(P-Cam));
''',additive=True)
material('EarthHalo',r'''
float facing=abs(dot(normalize(N),normalize(V)));
float rim=pow(1-facing,5);
float sun=.35+.65*saturate(dot(normalize(N),normalize(float3(-.45,-.6,.7))));
return float3(.06,.46,1.0)*rim*(1.4+sun*1.8);
''',additive=True)
material('CellInterior',r'''
float folds=.5+.5*sin(P.x*.006+sin(P.y*.009)*2)*sin(P.z*.009);
float skin=.60+.22*folds+.18*pow(saturate(abs(N.z)),2);
float speck=pow(saturate(sin(P.x*.065)*sin(P.y*.069)*sin(P.z*.051)),18);
return Tint.rgb*skin+float3(.085,.025,.040)*speck;
''')
material('Membrane',r'''
float2 p=P.yz*.017;
float a=pow(saturate(.5+.5*sin(p.x*2+sin(p.y*1.7))),12);
float b=pow(saturate(.5+.5*cos(p.y*2.8+sin(p.x))),18);
return float3(.42,.095,.12)*(.38+a*.6+b*.6)*(.92+.08*sin(T*1.7))+float3(.22,.115,.055)*(a+b);
''')
material('CosmicOrganelle',r'''
float3 n=normalize(N);
float face=.28+.62*saturate(dot(n,normalize(float3(-.6,-.35,1))));
float rim=pow(1-abs(dot(n,normalize(V))),3);
float grain=.85+.15*sin(P.y*.0007+sin(P.z*.0009)*2);
return Tint.rgb*face*grain+float3(.13,.055,.24)*rim;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2);
return (.52+rim*.28)*Reveal;
''')
material('StellarSurface',r'''
float3 n=normalize(N);
float flow=sin(n.x*23+sin(n.y*17+T*.13)*2)*sin(n.z*29+sin(n.x*19)*1.3);
float granules=.5+.5*sin(n.x*61+n.z*27)*sin(n.y*57-n.z*33);
float bands=smoothstep(-.6,.8,flow*.65+granules*.35);
float shade=.5+.5*saturate(dot(n,normalize(float3(-1,-.3,.5))));
return lerp(Tint.rgb*.40,float3(1,.82,.5),bands*.42)*shade*2.0*Reveal;
''')
material('StellarCorona',r'''
float2 p=(UV-.5)*2;float r=length(p);float a=atan2(p.y,p.x);
float edge=1-smoothstep(.6,1,r);
float halo=exp(-r*6)*.9;
float corona=exp(-abs(r-.25)*22)*(.45+.55*pow(.5+.5*sin(a*9+sin(a*5+T*.3)),3));
float rays=pow(saturate(1-abs(sin(a*2+.3))),32)*exp(-r*3)*.15;
return Tint.rgb*(halo+corona*.45+rays)*edge*Reveal;
''',additive=True)

material('CorticalTissue',r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float sulcus=1-smoothstep(.035,.16,C.r);
return float3(.020,.10,.28)*(.08+rim*1.15+sulcus*.4)*.65;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float sulcus=1-smoothstep(.035,.16,C.r);
return (.005+rim*.28+sulcus*.10)*smoothstep(100,350,length(P-Cam));
''')
material('Cortex',r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float groove=1-smoothstep(.035,.16,C.r);
return float3(.025,.12,.32)*(.12+rim*1.8+groove*.65)*.65;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float groove=1-smoothstep(.035,.16,C.r);
return (.008+rim*.42+groove*.16)*smoothstep(100,500,length(P-Cam));
''')
material('NeuronHero',r'''
float3 n=normalize(N),v=normalize(V);
float rim=pow(1-abs(dot(n,v)),2.2);
float shade=.25+.75*saturate(dot(n,normalize(float3(-.7,-.3,1))));
float spark=pow(saturate(sin(P.x*.31+sin(P.z*.42))*sin(P.y*.39-P.z*.21)),24);
return Tint.rgb*(.22*shade+rim*1.65)+float3(.45,.8,1)*spark*.18;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.2);
return (.13+rim*.62)*smoothstep(130,270,length(P-Cam))*Reveal;
''')
material('NeuronCore',r'''
float facing=abs(dot(normalize(N),normalize(V)));
float swirl=pow(.5+.5*sin(UV.x*19+sin(UV.y*14+T*.4)*2),8);
return lerp(Tint.rgb*.12,Tint.rgb*(.5+swirl*2),pow(facing,.7));
''',opacity=r'''
return .38*smoothstep(130,250,length(P-Cam))*Reveal;
''')
material('NeuronGlass',r'''
float3 n=normalize(N),v=normalize(V),key=normalize(float3(-.4,-.3,1));
float rim=pow(1-abs(dot(n,v)),2.2);
float shade=.18+.82*saturate(dot(n,key));
float glint=pow(saturate(dot(reflect(-key,n),v)),24);
float pulse=pow(saturate(.5+.5*sin(T*7+Obj.x*.013+Obj.y*.017+Obj.z*.009)),26)*Activity;
return Tint.rgb*(.20*shade+rim*1.3)+float3(.09,.22,.36)*glint*.45+float3(.35,.70,1)*pulse*2.4;
''',opacity=r'''
float nearFade=smoothstep(130,270,length(P-Cam));
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.2);
return (.055+rim*.62)*nearFade*Reveal;
''')
material('NeuralFilament',r'''
float3 n=normalize(N),v=normalize(V);
float rim=pow(1-abs(dot(n,v)),1.6);
float flow=.78+.22*sin(UV.y*25-T*3);
float glint=pow(saturate(dot(reflect(-normalize(float3(-.4,-.3,1)),n),v)),18);
// A grazing view along a neurite can make Fresnel uniform across its whole
// length. Keep fixed directional shading and bound the pulse's HDR peak so
// nearby branches do not turn into flat white support poles on arrival.
float shade=.22+.78*pow(.5+.5*dot(n,normalize(float3(-.4,-.3,1))),2);
float peak=max(Tint.r,max(Tint.g,Tint.b));
float3 energy=Tint.rgb/(1+peak*.4);
float signal=pow(saturate(.5+.5*cos(UV.y*6.283-T*9+Variant*5)),24)*Activity;
return energy*(.20+rim*1.7+glint*.2)*flow*shade+float3(.55,.85,1)*signal*3;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),1.6);
return (.06+rim*.56)*smoothstep(220,650,length(P-Cam))*Reveal;
''')
material('LivingMembrane',r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),2.6);
float flow=.5+.5*sin(UV.x*33+sin(UV.y*27+T*.25)*1.5);
float sheen=pow(saturate(dot(normalize(N),normalize(float3(-.6,-.2,1)))),12);
return lerp(Tint.rgb,float3(.34,.085,.24),flow*.28)*(.20+rim*1.05)+float3(.55,.22,.29)*sheen*.38;
''',opacity=r'''
float rim=pow(1-abs(dot(normalize(N),normalize(V))),3);
return (.018+rim*.48)*Reveal;
''',wpo=r'''
float a=UV.x*6.283185,b=UV.y*3.141593;
float wave=sin(a*3+T*.23)*sin(b*4+T*.17)+.35*sin(a*7-b*5-T*.13);
return VN*dot(abs(Scale),float3(.3333,.3333,.3333))*.8*wave*sin(b);
''')
material('SpaceField',r'''
float3 s=Texture2DSample(StarMap,StarMapSampler,UV).rgb;
float edge=smoothstep(0,.13,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));
return s*float3(.72,.82,1.2)*3*edge*Reveal;
''',additive=True,textures={'StarMap':stars})
material('NebulaField',r'''
float3 c=Texture2DSample(NebulaMap,NebulaMapSampler,UV).rgb;
float edge=smoothstep(0,.12,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));
return c*float3(.68,.83,1.05)*edge*.85*Reveal;
''',additive=True,textures={'NebulaMap':nebula})
material('CosmicStar','return Tint.rgb*Reveal;')
material('CellNebula',r'''
float r=length((UV-.5)*2);
float3 clouds=Texture2DSample(NebulaMap,NebulaMapSampler,UV).rgb;
float3 dots=Texture2DSample(StarMap,StarMapSampler,UV).rgb;
return (clouds*float3(.82,.75,1.22)*1.05+dots*.8)*(1-smoothstep(.50,.96,r))*Reveal;
''',additive=True,textures={'NebulaMap':nebula,'StarMap':stars})
material('SolarPlanet',r'''
float3 n=normalize(N);
float angle=Spin*6.2831853;
float3 local=float3(cos(angle)*n.x-sin(angle)*n.y,sin(angle)*n.x+cos(angle)*n.y,n.z);
float stripe=.5+.5*sin(local.z*(Variant>3?42:17)+sin(local.x*13+local.y*11)*.6);
float shade=.13+.87*smoothstep(-.2,.8,dot(n,normalize(float3(-1,-.3,.5))));
return Tint.rgb*lerp(.72,1.18,stripe)*shade;
''')
material('PlanetRing',r'''
float r=length((UV-.5)*2);
float ring=smoothstep(.45,.48,r)*(1-smoothstep(.90,.94,r));
float stripe=.45+.55*pow(.5+.5*sin(r*145),2);
return Tint.rgb*ring*stripe*.65;
''',additive=True)
exec(compile((project/'Scripts/apply_ending_night_earth.py').read_text(encoding='utf-8-sig'),'apply_ending_night_earth.py','exec'))

# Rebuild the cinematic lenses and finish materials, even if older assets exist.
exec(compile((project/'Scripts/apply_ending_drill_comic.py').read_text(encoding='utf-8-sig'),'apply_ending_drill_comic.py','exec'))
exec(compile((project/'Scripts/apply_ending_camera_finish.py').read_text(encoding='utf-8-sig'),'apply_ending_camera_finish.py','exec'))
exec(compile((project/'Scripts/apply_ending_paper_finale.py').read_text(encoding='utf-8-sig'),'apply_ending_paper_finale.py','exec'))

assert levels.load_level(root+'/Maps/Lvl_EndingEmergence_Demo')
demo=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
demo.build_demo()
assert levels.save_current_level()
unreal.log('ENDING_ORGANIC_ART_READY: compact cell, cortex, translucent neurons, night Earth, layered stars, living membranes')
