"""Save only the feedback's subtitle, footprint and supplied choir assets."""
import hashlib,json,shutil,sys,traceback,wave
from array import array
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Scripts'))
from scene_polish_materials import mat,node,custom,prop,finish
EA=unreal.EditorAssetLibrary
out=root/'Saved/Validation/Playtest18';out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'subtitles':[]}

def protect(path):
    filename=root/'Content'/(path.split('.')[0][6:]+'.uasset')
    backup=out/'Before'/filename.relative_to(root)
    if filename.exists() and not backup.exists():
        backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(filename,backup)

def save(asset):assert EA.save_loaded_asset(asset,False)

try:
    for path,expected in [('/Game/Subtitles/DA_Cards2',['这种卡牌玩法会不会太常见了呢？']),
                          ('/Game/Subtitles/DA_RisingWater',['水？上涌的水…不断地跳']),
                          ('/Game/Subtitles/Collectibles/DA_CollectiblePickup_11',['好漂亮的彩带','继续往前走走'])]:
        asset=unreal.load_asset(path);assert isinstance(asset,unreal.SubtitleSegment),path
        protect(path)
        lines=[]
        for text in expected:
            line=unreal.SubtitleLine();line.set_editor_property('text',text);lines.append(line)
        asset.set_editor_property('lines',lines);save(asset)
        assert [str(x.text) for x in asset.lines]==expected
        report['subtitles'].append(dict(path=path,lines=expected))

    path='/Game/MainGame/Materials/M_PlayerFootprint';protect(path);material=mat(path)
    material.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=node(material,unreal.MaterialExpressionTextureCoordinate)
    side=node(material,unreal.MaterialExpressionScalarParameter,parameter_name='FootSide',default_value=1.)
    life=node(material,unreal.MaterialExpressionDecalLifetimeOpacity)
    # A rounded forefoot, narrowed instep and separate heel. Mirroring the
    # medial arch distinguishes left and right; shallow tread gives a shoe print.
    code='''
float2 p=float2((1-UV.y*2)*Side,UV.x*2-1);
float toe=1-smoothstep(.96,1.03,length((p-float2(-.06,.35))/float2(.76,.56)));
float waist=1-smoothstep(.95,1.03,length((p-float2(-.15,-.08))/float2(.47,.51)));
float heel=1-smoothstep(.94,1.03,length((p-float2(-.02,-.62))/float2(.53,.29)));
float sole=max(toe,max(waist,heel));
float arch=1-smoothstep(.36,.43,length((p-float2(.72,-.19))/float2(1.,1.15)));
sole*=1-arch;
float heelGap=1-smoothstep(.025,.045,abs(p.y+.34));
float grooves=(1-smoothstep(.035,.065,abs(frac((p.y+.11)*5.)-.5)))*step(-.10,p.y);
return saturate(sole*(1-heelGap)*(1-.30*grooves));
'''
    shape=custom(material,code,{'UV':(uv,''),'Side':(side,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    alpha=custom(material,'return .38*Shape*saturate(Life);',{'Shape':(shape,''),'Life':(life,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    color=custom(material,'return float3(.32,.34,.35)*Shape*saturate(Life);',{'Shape':(shape,''),'Life':(life,'')})
    prop(alpha,unreal.MaterialProperty.MP_OPACITY);prop(color,unreal.MaterialProperty.MP_EMISSIVE_COLOR);finish(material)
    report['footprint']='Mirrored shoe soles with forefoot, instep, heel gap and shallow tread; original fade and brightness retained'

    source=root/'SourceAssets/RoomAudio/Wav/S_HeavenAngelChoir.wav'
    assert source.exists()
    with wave.open(str(source),'rb') as wav:
        channels=wav.getnchannels();width=wav.getsampwidth();rate=wav.getframerate();frames=wav.readframes(wav.getnframes())
    if channels==2:
        assert width==2,'Expected signed 16-bit PCM choir'
        samples=array('h',frames)
        mono=array('h',((int(samples[i])+int(samples[i+1]))//2 for i in range(0,len(samples),2)))
        with wave.open(str(source),'wb') as wav:
            wav.setnchannels(1);wav.setsampwidth(2);wav.setframerate(rate);wav.writeframes(mono.tobytes())
    path='/Game/Audio/Rooms/S_HeavenAngelChoir';protect(path)
    task=unreal.AssetImportTask();task.filename=str(source);task.destination_path='/Game/Audio/Rooms'
    task.destination_name='S_HeavenAngelChoir';task.automated=True;task.replace_existing=True;task.save=False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound=unreal.load_asset(path);assert isinstance(sound,unreal.SoundWave)
    sound.set_editor_property('looping',False);sound.set_editor_property('volume',1.)
    sound.set_editor_property('sound_class_object',unreal.load_asset('/Game/Audio/Mix/SC_Effects'))
    concurrency=unreal.SoundConcurrencySettings();concurrency.set_editor_property('max_count',1)
    concurrency.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
    sound.set_editor_property('override_concurrency',True);sound.set_editor_property('concurrency_overrides',concurrency)
    save(sound)
    with wave.open(str(source),'rb') as wav:
        channels=wav.getnchannels();duration=wav.getnframes()/wav.getframerate()
    assert channels==1 and not sound.get_editor_property('looping')
    report['choir']=dict(asset=path,seconds=duration,channels=channels,sha256=hashlib.sha256(source.read_bytes()).hexdigest())
    manifest=root/'SourceAssets/RoomAudio/manifest.json'
    entries=json.loads(manifest.read_text(encoding='utf8'))
    entries=[entry for entry in entries if entry['asset']!='S_HeavenAngelChoir']
    entries.append(dict(asset='S_HeavenAngelChoir',file='Wav/S_HeavenAngelChoir.wav',sha256=report['choir']['sha256'],loop=False,category='Effects',max_count=1,channels=channels,seconds=duration,sources=[dict(pack='user',member='Originals/User/dragon-studio-angel-choir-463220.mp3')]))
    manifest.write_text(json.dumps(entries,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    report['passed']=True
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
(out/'assets-applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
