"""Ending-only luminous streams and sparks; no water refraction or wet screen."""
import unreal, math, wave, random, array
from pathlib import Path
assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
root='/Game/EndingEmergence'
def make(name,code):
    path=root+'/Materials/M_EE_'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_'+name,root+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('two_sided',True)
    bindings=[]
    for key,cls in [('UV',unreal.MaterialExpressionTextureCoordinate),('Time',unreal.MaterialExpressionTime),('Seed',unreal.MaterialExpressionScalarParameter),('Flow',unreal.MaterialExpressionScalarParameter),('Tint',unreal.MaterialExpressionVectorParameter)]:
        n=lib.create_material_expression(m,cls)
        if key in ['Seed','Flow','Tint']:
            n.set_editor_property('parameter_name',key)
            n.set_editor_property('default_value',unreal.LinearColor(1,.65,.2,1) if key=='Tint' else 1. if key=='Flow' else 0.)
        bindings.append((key,n))
    custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for key,_ in bindings:
        i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
    custom.set_editor_property('inputs',inputs);custom.set_editor_property('code',code)
    for key,n in bindings:assert lib.connect_material_expressions(n,'',custom,key)
    for prop,channels in [(unreal.MaterialProperty.MP_EMISSIVE_COLOR,(True,True,True,False)),(unreal.MaterialProperty.MP_OPACITY,(False,False,False,True))]:
        mask=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask)
        for c,v in zip('rgba',channels):mask.set_editor_property(c,v)
        assert lib.connect_material_expressions(custom,'',mask,'')
        assert lib.connect_material_property(mask,'',prop)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
make('StarlightStream',r'''
float2 q=float2(UV.x*9+Seed*23,UV.y*42-Time*(11+Seed*3));
float2 id=floor(q),d=frac(q)-.5;
float h=frac(sin(dot(id,float2(12.9898,78.233))+Seed*137)*43758.5453);
float core=exp(-dot(d,d)*190);
float cross=(exp(-abs(d.x)*85-abs(d.y)*10)+exp(-abs(d.y)*85-abs(d.x)*10))*.32;
float stars=(core+cross)*step(.30,h);
float filaments=pow(saturate(.5+.5*sin(UV.x*87+Seed*29+sin(UV.y*13-Time*4)*.7)),22)*.07;
float edges=pow(saturate(sin(UV.x*3.14159)),.8)*smoothstep(0,.07,UV.y)*(1-smoothstep(.78,1,UV.y));
float3 col=lerp(float3(1,.48,.10),float3(.23,.67,1),step(.67,h));
return float4(col*(stars*5+filaments)*Flow,edges*saturate(stars+filaments)*Flow);
''')
make('StarlightSpark',r'''
float shimmer=.7+.3*sin(Time*11+Seed*39);
return float4(Tint.rgb*(3.5+shimmer*2),.8*Flow);
''')
# Soft airy onset and scattered bell-like harmonics, replacing the water recording.
out=Path(unreal.Paths.project_dir())/'SourceAssets/EndingEmergence/Audio/S_EE_StarlightBurst.wav'
out.parent.mkdir(parents=True,exist_ok=True)
rate=24000;seconds=5.6;data=[0.]*(int(rate*seconds));rng=random.Random(605)
for k in range(45):
    start=rng.uniform(0,4.8);freq=rng.choice([659.25,783.99,987.77,1174.66,1318.51])*rng.choice([1,1,2])
    amplitude=.035*(1-start/seconds);length=min(1.,seconds-start)
    for j in range(int(length*rate)):
        t=j/rate;env=min(1,t/.008)*math.exp(-t*6)
        data[int(start*rate)+j]+=amplitude*env*(math.sin(t*freq*math.tau)+.28*math.sin(t*freq*2.01*math.tau))
previous=0
for i in range(len(data)):
    t=i/rate;previous=.94*previous+.06*rng.uniform(-1,1)
    data[i]+=.22*previous*min(1,t/.06)*math.exp(-t*1.4)
samples=array.array('h',(int(max(-1,min(1,x))*28000) for x in data))
with wave.open(str(out),'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(rate);f.writeframes(samples.tobytes())
task=unreal.AssetImportTask();task.filename=str(out);task.destination_path=root+'/Audio';task.automated=True;task.replace_existing=True;task.save=True
assets.import_asset_tasks([task]);assert task.imported_object_paths
unreal.log('ENDING_STARLIGHT_APPLIED')
