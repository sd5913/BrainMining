"""NASA Black Marble lights on the ending Earth; preserves the camera rail.
Can run standalone in a clean editor or from apply_ending_organic_art.py.
"""
import unreal
from pathlib import Path

if 'material' not in globals():
    project = Path(unreal.Paths.project_dir()).resolve()
    bootstrap = (project/'Scripts/apply_ending_organic_art.py').read_text(encoding='utf-8-sig').split('# Closed shared meshes')[0]
    exec(compile(bootstrap, 'ending_material_helpers', 'exec'))

planet_textures = {}
for key, name in {'DayMap':'T_8k_earth_daymap', 'CloudMap':'T_8k_earth_clouds', 'ReliefMap':'T_8k_earth_normal_map', 'OceanMap':'T_8k_earth_specular_map'}.items():
    source = unreal.load_asset('/Game/Planet_Project/Assets/Textures/Earth/'+name)
    assert source, name
    path = root+'/Planet/T_EE_Clear'+key
    tex = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.duplicate_asset('T_EE_Clear'+key, root+'/Planet', source)
    tex.set_editor_property('max_texture_size', 4096 if key=='CloudMap' else 8192)
    tex.set_editor_property('lod_bias', 0)
    tex.set_editor_property('never_stream', True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    planet_textures[key] = tex
    unreal.log('EARTH_SOURCE '+key+' '+str(tex.blueprint_get_size_x())+'x'+str(tex.blueprint_get_size_y()))

for key, name in [('NightMap','T_NASA_NightGlobal'), ('NightDetail','T_NASA_NightEurope')]:
    path = root+'/Planet/'+name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = str(project/'SourceAssets/EndingEmergence/ThirdParty/NASA'/(name+'.png'))
        task.destination_path = root+'/Planet'; task.destination_name = name
        task.automated = True; task.save = True
        assets.import_asset_tasks([task])
        assert task.imported_object_paths, name
    tex = unreal.load_asset(path)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('max_texture_size',8192)
    tex.set_editor_property('lod_bias',0)
    tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_SHARPEN2)
    tex.set_editor_property('never_stream',True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
    planet_textures[key] = tex
    unreal.log('EARTH_NASA '+name+' '+str(tex.blueprint_get_size_x())+'x'+str(tex.blueprint_get_size_y()))

material('Planet', r'''
// Analytic sphere coordinates avoid low-poly vertex-normal interpolation warping the maps.
float3 n=normalize(P-Obj);
// Face Europe to the existing pullback camera; all surface layers share this rotation.
// Fixed 23.44 degree north axis. Texture longitude increases eastward, opposite atan2.
float3 g=float3(.917477*n.x+.397789*n.z,n.y,-.397789*n.x+.917477*n.z);
float2 q=float2(frac(-atan2(g.y,g.x)/6.2831853+.075-Spin),acos(clamp(g.z,-1,1))/3.1415927);
float3 day=Texture2DSample(DayMap,DayMapSampler,q).rgb;
float cloud=Texture2DSample(CloudMap,CloudMapSampler,q+float2(T*.000025,0)).r;
float lights=Texture2DSample(NightMap,NightMapSampler,q).r;
float2 detailUV=float2((q.x-.4375)*4,q.y*2);
float edgeDist=min(min(detailUV.x,1-detailUV.x),min(detailUV.y,1-detailUV.y));
float detailWeight=smoothstep(0,.025,edgeDist);
float fine=Texture2DSample(NightDetail,NightDetailSampler,saturate(detailUV)).r;
lights=lerp(lights,fine,detailWeight);
// Preserve faint connecting lights and the brightest urban cores as separate levels.
float dim=pow(max(lights-.001,0),.88);
float core=pow(lights,2.1);
float3 lamps=float3(1,.43,.12)*dim*2.7+float3(1,.72,.35)*core*3.2;
float land=smoothstep(.008,.045,day.r+day.g*.45-day.b*.80);
// Relief normals follow the same rotating geographic frame as the night lights.
float2 bump=Texture2DSample(ReliefMap,ReliefMapSampler,q).rg*2-1;
float3 east=normalize(float3(g.y,-g.x,0));
float3 south=normalize(cross(east,g));
float3 texturedG=normalize(g+east*bump.x*.65+south*bump.y*.65);
float3 surfaceN=normalize(float3(.917477*texturedG.x-.397789*texturedG.z,texturedG.y,.397789*texturedG.x+.917477*texturedG.z));
// Match the visible world-space photosphere, placed behind the upper limb.
float3 sunDir=normalize(float3(2200,-650,3850)-n*4000);
float sun=saturate(dot(surfaceN,sunDir));
float softDay=smoothstep(-.35,.45,dot(n,sunDir));
float ocean=Texture2DSample(OceanMap,OceanMapSampler,q).r;
float relief=pow(saturate(dot(day,float3(.25,.55,.20))*3),.7);
float3 ground=lerp(float3(.001,.004,.012),float3(.012,.028,.054)+day*float3(.11,.16,.24),land);
float skyFill=pow(saturate(dot(surfaceN,normalize(float3(-.10,-.55,1)))),2);
ground*=.24+skyFill*.8+softDay*(.55+sun*2.2);
float cloudShadow=Texture2DSample(CloudMap,CloudMapSampler,q+float2(.002,-.0015)+float2(T*.000025,0)).r;
ground*=1-smoothstep(.2,.8,cloudShadow)*.48;
float c=smoothstep(.20,.80,cloud);
float facing=saturate(dot(n,normalize(V)));
float rim=pow(1-facing,6);
float sourceArc=pow(saturate(dot(n,normalize(float3(2200,-650,3850)))),18);
float3 twilight=float3(.035,.24,.60);
float oceanGlint=(pow(saturate(dot(reflect(-sunDir,surfaceN),normalize(V))),65)*.55+pow(saturate(dot(reflect(-normalize(float3(-.65,-.1,1)),surfaceN),normalize(V))),24)*.075)*ocean;
float globeShade=.58+.42*pow(facing,.6);
return (ground+lamps*(1-c*.28)*(1-softDay*.28))*globeShade+twilight*rim*(.025+sourceArc*.7)+float3(.35,.48,.68)*oceanGlint;
''', textures=planet_textures)

material('EarthHalo', r'''
float3 n=normalize(P-Obj);
float facing=abs(dot(n,normalize(V)));
// Fade at the outer silhouette, not a hard cyan cutout around the sphere.
float outer=smoothstep(0,.15,facing)*exp(-facing*facing*26);
float sun=saturate(dot(n,normalize(float3(2200,-650,3850))));
float arc=pow(sun,14);
float3 dawn=lerp(float3(.04,.29,.74),float3(.7,.48,.28),pow(sun,40)*.55);
return dawn*outer*(.006+arc*.65);
''', additive=True)
material('EarthDawn',r'''
float2 p=(UV-.5)*2;
float r=length(p),a=atan2(p.y,p.x);
float edge=1-smoothstep(.65,1,r);
float core=exp(-r*r*850)*10;
float glow=exp(-r*12)*.85;
float rays=pow(saturate(.5+.5*cos(a*7+.18+.4*sin(a*3))),24)*exp(-r*5)*smoothstep(.025,.09,r)*.055;
float streak=exp(-abs(p.y)*120)*exp(-abs(p.x)*6)*.14;
return (float3(1,.67,.32)*(core+glow+rays)+float3(.32,.47,.75)*streak)*edge;
''',additive=True)
cloud_uv=r'''
float3 n=normalize(P-Obj);
float3 g=float3(.917477*n.x+.397789*n.z,n.y,-.397789*n.x+.917477*n.z);
float2 q=float2(frac(-atan2(g.y,g.x)/6.2831853+.075-Spin-T*.000055),acos(clamp(g.z,-1,1))/3.1415927);
float c=Texture2DSample(CloudMap,CloudMapSampler,q).r;
float sun=saturate(dot(n,normalize(float3(2200,-650,3850)-n*4037.5)));
float fill=saturate(dot(n,normalize(float3(-.65,-.1,1))));
'''
material('EarthClouds',cloud_uv+r'''
float c2=Texture2DSample(CloudMap,CloudMapSampler,q+float2(.001,-.001)).r;
float relief=saturate(.5+(c-c2)*3);
return lerp(float3(.028,.06,.115)*(1+fill*.6),float3(.45,.58,.72),pow(sun,.7))*(.4+relief);
''',opacity=cloud_uv+r'''
return smoothstep(.14,.78,c)*(.28+sun*.38);
''',textures={'CloudMap':planet_textures['CloudMap']})
unreal.log('ENDING_NASA_EARTH_READY: Black Marble global + 500m-derived Europe detail, continuous city lights and soft atmosphere')
