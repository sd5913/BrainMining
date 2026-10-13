"""Generate the isolated ending map, materials and synthesized temp soundscape.
Run in a dedicated UnrealEditor-Cmd with -ExecutePythonScript. Existing map is
backed up before an explicit rebuild (set ENDING_REBUILD=1 in the environment).
No campaign/default-map/character assets are modified.
"""
import unreal
import math
import random
import wave
import struct
import os
import shutil
import datetime
from pathlib import Path

ROOT = '/Game/EndingEmergence'
MAP = ROOT + '/Maps/Lvl_EndingEmergence_Demo'
assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
if exists and os.environ.get('ENDING_REBUILD') != '1':
    raise RuntimeError('Map already exists. Set ENDING_REBUILD=1 for a backed-up rebuild.')
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
    raise RuntimeError('Use a clean dedicated editor; unsaved work must be preserved.')
if exists:
    backup = Path(unreal.Paths.project_saved_dir()) / 'Backups' / ('EndingEmergence-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(unreal.Paths.project_content_dir()) / (MAP.removeprefix('/Game/')+'.umap'), backup)


def material(name, code, tint=(.2, .4, .4), additive=False):
    path = ROOT + '/Materials/M_EE_' + name
    # Reuse untouched assets, including those held open by the user's editor.
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_' + name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_OPAQUE)
    def node(cls): return lib.create_material_expression(m, cls)
    custom = node(unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name_ in ['P', 'N', 'UV', 'Tint', 'T', 'V']:
        inp = unreal.CustomInput(); inp.set_editor_property('input_name', name_); inputs.append(inp)
    custom.set_editor_property('inputs', inputs); custom.set_editor_property('code', code)
    pos = node(unreal.MaterialExpressionWorldPosition)
    normal = node(unreal.MaterialExpressionPixelNormalWS)
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    color = node(unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Tint'); color.set_editor_property('default_value', unreal.LinearColor(*tint, 1))
    time = node(unreal.MaterialExpressionTime)
    view = node(unreal.MaterialExpressionCameraVectorWS)
    for source, output, target in [(pos, 'XYZ', 'P'), (normal, '', 'N'), (uv, '', 'UV'), (color, '', 'Tint'), (time, '', 'T'), (view, '', 'V')]:
        lib.connect_material_expressions(source, output, custom, target)
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m); assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


material('Surface', '''
float light=.4+.6*saturate(dot(normalize(N),normalize(float3(-.7,-.4,1))));
float grain=.97+.03*sin(P.x*.11)*sin(P.y*.13)*sin(P.z*.14);
return Tint.rgb*light*grain;
''')
material('Glow', 'return Tint.rgb*2.8;')
material('CellInterior', '''
float folds=.5+.5*sin(P.x*.006+sin(P.y*.009)*2)*sin(P.z*.009);
float skin=.64+.22*folds+.14*pow(saturate(abs(N.z)),2);
float speck=pow(saturate(sin(P.x*.065)*sin(P.y*.069)*sin(P.z*.051)),18);
return Tint.rgb*skin+float3(.015,.065,.050)*speck;
''')
material('Vessel', '''
float pulse=.85+.15*sin(T*5.2-P.x*.002);
float fibres=.5+.5*sin(P.x*.025+sin(P.y*.015+P.z*.009)*2);
float streaming=pow(saturate(.5+.5*sin(P.x*.009-T*5.1)),12);
return Tint.rgb*(.52+fibres*.2+streaming*.28)*pulse+float3(.045,.001,.003);
''')
material('BrainGround', '''
float folds=sin(P.x*.008+sin(P.y*.006)*3)*sin(P.y*.009+sin(P.x*.004)*2);
float grooves=pow(saturate(1-abs(folds)),12);
float light=.45+.55*saturate(dot(normalize(N),normalize(float3(-.6,-.4,1))));
return Tint.rgb*(.7+.3*folds)*light+float3(.018,.045,.042)*grooves;
''')
material('RedMembrane', '''
float wave=pow(saturate(.5+.5*sin(P.y*.035+sin(P.z*.026)*2)),10);
return Tint.rgb*(.65+wave*.5)*(.85+.15*sin(T*5.2));
''', (.55,.028,.035))
material('Organic', '''
float fold=sin(P.x*.015+sin(P.z*.012)*2)*sin(P.y*.019+sin(P.z*.021));
float light=.45+.55*saturate(dot(normalize(N),normalize(float3(-.6,-.3,1))));
return Tint.rgb*(.7+.3*fold)*light;
''')
material('Blood', '''
float light=.4+.6*saturate(dot(normalize(N),normalize(float3(-.7,-.5,.8))));
return Tint.rgb*light*(1-.45*pow(saturate(abs(N.z)),8));
''')
material('Membrane', '''
float2 p=P.yz*.017;
float a=pow(saturate(.5+.5*sin(p.x*2+sin(p.y*1.7))),12);
float b=pow(saturate(.5+.5*cos(p.y*2.8+sin(p.x))),18);
return Tint.rgb*(.38+a*.6+b*.6)*(.92+.08*sin(T*1.7))+float3(.12,.065,.017)*(a+b);
''', (.06,.42,.37))
material('Planet', '''
float2 q=UV*float2(6.283185,3.141593);
float land=sin(q.x*3+sin(q.y*4)*2)+.6*sin(q.x*7-q.y*5)+.35*sin(q.x*13+q.y*9);
float coast=smoothstep(.18,.35,land);
float ice=smoothstep(.83,.97,abs(cos(q.y)));
float3 c=lerp(Tint.rgb,float3(.06,.19,.12),coast);
c=lerp(c,float3(.6,.75,.77),ice);
float cloud=pow(saturate(.5+.5*sin(q.x*11+q.y*8+sin(q.y*17)*2)),16)*.28;
c=lerp(c,float3(.7,.8,.9),cloud);
float lit=saturate(dot(normalize(N),normalize(float3(-.2,-.6,1))));
float towns=pow(saturate(sin(q.x*281)*sin(q.y*213)),22)*coast*(1-lit)*.8;
return c*(.11+.89*lit)+float3(1,.48,.07)*towns;
''')
material('Atmosphere', 'return Tint.rgb*pow(1-abs(dot(normalize(N),normalize(V))),3)*1.8;', additive=True)
material('Haze', '''
float soft=pow(saturate(abs(dot(normalize(N),normalize(V)))),2);
float cloud=.2+.8*pow(saturate(.5+.5*sin(UV.x*35+sin(UV.y*28)*2)),3);
return Tint.rgb*soft*cloud*.35;
''', additive=True)

# Continental night view: coherent coastline, clustered settlements and broken
# cloud decks. World scale differs from the street tableau deliberately.
noise = '''
struct Field {
    float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
    float n(float2 p) {
        float2 i=floor(p), f=frac(p); f=f*f*(3-2*f);
        return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
    }
    float fb(float2 p) { return .52*n(p)+.26*n(p*2.03+17)+.13*n(p*4.07+31)+.065*n(p*8.13); }
}; Field f;
'''
material('NightContinent', noise + '''
float2 p=(UV-.5)*8;
float coast=p.x*.7+p.y*.25+.45*sin(p.y*2.1)+.23*sin(p.y*5.3)+.65*(f.fb(p*3)-.5)-.15;
float land=smoothstep(-.025,.035,coast);
float2 bend=p+float2(f.fb(p*2.7),f.fb(p*3.1+50))*.45;
float settlement=pow(saturate(f.fb(bend*8)*1.6-.35),3);
float coastal=exp(-abs(coast-.08)*18);
float urban=saturate(settlement*1.8+coastal*.32);
float2 grid=bend*380;
float2 cell=floor(grid), d=frac(grid)-.5;
float stars=exp(-dot(d,d)*44)*step(.54-urban*.27,f.hash(cell));
float roads=pow(saturate(1-abs(sin(bend.x*115+sin(bend.y*24)*2))),24)
           +pow(saturate(1-abs(sin(bend.y*142+sin(bend.x*19)*2))),28);
float lights=land*(stars*urban*4.2+roads*urban*.11);
float cityGlow=land*pow(urban,2)*.12;
float3 sea=float3(.002,.008,.020)*(1+.35*f.fb(p*12));
float3 ground=float3(.008,.016,.025)*( .7+.5*f.fb(p*16));
return lerp(sea,ground,land)+float3(1,.49,.15)*lights+float3(.48,.24,.075)*cityGlow;
''')
material('CloudDeck', noise + '''
float2 p=(UV-.5)*9;
p+=float2(T*.0009,T*.0004);
float warp=f.fb(p*1.2);
float body=f.fb(p*3.3+float2(warp*2,warp));
float cloud=smoothstep(.44,.72,body);
float wisps=f.fb(p*24);
float edge=saturate((.48-max(abs(UV.x-.5),abs(UV.y-.5)))*12);
return Tint.rgb*cloud*(.6+wisps*.6)*edge;
''', additive=True)

# Actual radial lens distortion, blended only on the local gameplay camera in the brain.
material('Iris', '''
float angle=atan2(N.z,N.y);
float fibres=.55+.3*sin(angle*91+N.x*4)+.15*sin(angle*143-N.x*7);
float rim=.35+.65*pow(saturate(abs(N.x)),.5);
return Tint.rgb*(fibres+.25)*rim;
''')

path = ROOT + '/Materials/M_EE_Fisheye'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_Fisheye', ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
    m.set_editor_property('blendable_location', unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    uv = lib.create_material_expression(m, unreal.MaterialExpressionScreenPosition)
    scene = lib.create_material_expression(m, unreal.MaterialExpressionSceneTexture)
    scene.set_editor_property('scene_texture_id', unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    custom = lib.create_material_expression(m, unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ['UV', 'SC']:
        i = unreal.CustomInput(); i.set_editor_property('input_name', n); ins.append(i)
    custom.set_editor_property('inputs', ins)
    custom.set_editor_property('code', '''
    float2 p=UV-.5;
    float r=dot(p,p);
    float2 warped=.5+p*(1+.38*r)/1.12;
    float2 sampleUV=ViewportUVToSceneTextureUV(saturate(warped),14);
    return SceneTextureLookup(sampleUV,14,false).rgb+SC.rgb*.00000001;
    ''')
    lib.connect_material_expressions(uv, 'ViewportUV', custom, 'UV')
    lib.connect_material_expressions(scene, 'Color', custom, 'SC')
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m); unreal.EditorAssetLibrary.save_loaded_asset(m)

# Original placeholder sound design: no speech synthesis or external assets.
audio_dir = Path(unreal.Paths.project_dir()) / 'SourceAssets' / 'EndingEmergence' / 'Audio'
audio_dir.mkdir(parents=True, exist_ok=True)
rate = 22050
for name, seconds, loop in [('Cell',12,True),('Blood',12,True),('Brain',12,True),('Street',12,True),('Space',16,True),('Afternoon',12,True),('Hit',.3,False),('Connect',1.6,False),('Pencil',2.8,False),('Wake',.55,False)]:
    if unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Audio/S_EE_'+name):
        continue
    rng = random.Random(77); frames = bytearray(); filtered = 0
    for idx in range(int(rate * seconds)):
        t=idx/rate; noise=rng.uniform(-1,1); filtered=.975*filtered+.025*noise
        if name=='Cell':
            phase=t%1.2
            env=math.exp(-phase*22)+.65*math.exp(-max(0,phase-.19)*30)*(phase>.19)
            v=.14*math.sin(t*2*math.pi*55)*env+.12*filtered
        elif name=='Blood': v=(.18*math.sin(2*math.pi*48*t)*math.exp(-(t%.9)*12)+filtered*.36)
        elif name=='Brain':
            v=.06*math.sin(2*math.pi*110*t)+.028*math.sin(2*math.pi*165*t)+.02*math.sin(2*math.pi*220*t)+filtered*.08
            v*=.65+.35*math.sin(math.pi*t/3)**2
        elif name=='Street': v=.55*filtered+.045*math.sin(2*math.pi*80*t)*math.sin(math.pi*t/4)**4
        elif name=='Space': v=.05*math.sin(2*math.pi*55*t)+.026*math.sin(2*math.pi*82.5*t)+.018*math.sin(2*math.pi*138*t)+filtered*.06
        elif name=='Afternoon': v=filtered*.22+.016*math.sin(2*math.pi*60*t)+.004*math.sin(2*math.pi*120*t)
        elif name=='Hit': v=(.28*noise+.3*math.sin(2*math.pi*130*t))*math.exp(-t*28)
        elif name=='Connect': v=(.15*math.sin(2*math.pi*440*t)+.07*math.sin(2*math.pi*660*t))*math.exp(-t*3)*min(1,t*80)
        elif name=='Wake': v=filtered*2.2*math.exp(-t*7)*min(1,t*35)
        else: v=noise*.17*(.5+.5*math.sin(t*19))*min(1,t*40)*min(1,(seconds-t)*8)
        # Fade the loop seam so placeholder ambience stays free of clicks.
        v*=min(1,t*15,(seconds-t)*15)
        frames.extend(struct.pack('<h', int(max(-.95,min(.95,v))*32767)))
    source=audio_dir/('S_EE_'+name+'.wav')
    with wave.open(str(source),'wb') as f:
        f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(frames)
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(source));task.set_editor_property('destination_path',ROOT+'/Audio')
    task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
    assets.import_asset_tasks([task])
    sound=unreal.load_asset(ROOT+'/Audio/S_EE_'+name)
    assert sound, name
    sound.set_editor_property('looping',loop);unreal.EditorAssetLibrary.save_loaded_asset(sound)

# Local Windows Chinese voice placeholder; the source WAV ships with the demo.
voice_source=audio_dir/'S_EE_IdeaLine.wav'
if voice_source.exists() and not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/Audio/S_EE_IdeaLine'):
    task=unreal.AssetImportTask()
    for key,value in [('filename',str(voice_source)),('destination_path',ROOT+'/Audio'),('automated',True),('save',True)]:
        task.set_editor_property(key,value)
    assets.import_asset_tasks([task])
    assert unreal.load_asset(ROOT+'/Audio/S_EE_IdeaLine')

unreal.EditorAssetLibrary.make_directory(ROOT+'/Maps')
assert (levels.load_level(MAP) if exists else levels.new_level(MAP))
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',unreal.EndingEmergenceGameMode)
existing=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.EndingEmergence)]
assert len(existing)<=1
demo=existing[0] if existing else actors.spawn_actor_from_class(unreal.EndingEmergence,unreal.Vector())
demo.set_actor_label('涌现 · 那个下午 / Ending Emergence');demo.build_demo()
starts=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
if not starts: starts=[actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(100,0,-179),unreal.Rotator())]
for start in starts: start.set_actor_location(unreal.Vector(100,0,-179),False,False)
volumes=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.PostProcessVolume)]
volume=volumes[0] if volumes else actors.spawn_actor_from_class(unreal.PostProcessVolume,unreal.Vector())
volume.set_editor_property('unbound',True)
settings=volume.get_editor_property('settings')
for name,value in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',0.0),('auto_exposure_max_brightness',0.0),('override_auto_exposure_bias',True),('auto_exposure_bias',0.0),('override_bloom_intensity',True),('bloom_intensity',.32),('override_motion_blur_amount',True),('motion_blur_amount',0.0)]:
    settings.set_editor_property(name,value)
volume.set_editor_property('settings',settings)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(160,0,-100),unreal.Rotator(0,0,0))
assert len(demo.gates)==5
assert levels.save_current_level()
unreal.log('ENDING_EMERGENCE_CREATED '+MAP)
