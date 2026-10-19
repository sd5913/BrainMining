"""Explicit polish asset edits. Preserve the editable UMG tree and shared MainGame map."""
import json, shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/GamePolish'
backup=out/'Before';backup.mkdir(parents=True,exist_ok=True)
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
def protect(path):
    filename=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
    if filename.exists():
        dest=backup/filename.relative_to(root/'Content');dest.parent.mkdir(parents=True,exist_ok=True)
        if not dest.exists():shutil.copy2(filename,dest)
def save(obj):
    assert assets.save_loaded_asset(obj,False),obj.get_path_name()
def expression(mat,kind,**props):
    obj=lib.create_material_expression(mat,kind)
    for k,v in props.items():obj.set_editor_property(k,v)
    return obj
def connect(a,b,pin,output=''):
    assert lib.connect_material_expressions(a,output,b,pin)
def scalar(mat,name,default):
    return expression(mat,unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=default)

for name,segment_id,text in [
    ('FirstCollectionHint','首次收藏提示','Collection……按C键打开界面吧'),
    ('StartRoomRobot','StartRoomView_Robot','你好。'),
    ('StartRoomComputer','StartRoomView_Computer','12点了，是时候了……'),
    ('StartRoomBook','StartRoomView_Book','有新东西的话就记录下吧')]:
    path='/Game/Subtitles/DA_'+name
    segment=unreal.load_asset(path)
    if not segment:
        factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',unreal.SubtitleSegment)
        segment=tools.create_asset('DA_'+name,'/Game/Subtitles',unreal.SubtitleSegment,factory)
    segment.set_editor_property('segment_id',segment_id)
    line=unreal.SubtitleLine();line.set_editor_property("text",text);line.set_editor_property("audio",None);segment.set_editor_property("lines",[line]);save(segment)

folder='/Game/CollectionPrototype/UI/Redesign'
source=root/'SourceAssets/CollectionInspect/Cropped/T_CollectionOwnedGray.png'
task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=folder;task.automated=True;task.replace_existing=True;task.save=True
tools.import_asset_tasks([task])
texture=unreal.load_asset(folder+'/T_CollectionOwnedGray')
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS);texture.set_editor_property('never_stream',True);save(texture)

path='/Game/CollectionPrototype/UI/WBP_CollectionInspect';protect(path)
bp=unreal.load_asset(path)
widgets={w.get_name():w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer() and w.get_outer().get_outer()==bp}
widgets['CollectionTitle'].set_text('收藏')
# Apply deltas once, retaining every other designer adjustment.
marker=assets.get_metadata_tag(bp,'GamePolish20261020')
if marker!='1':
    panel=widgets['PreviewPanel'];p=panel.slot.get_position();panel.slot.set_position(unreal.Vector2D(p.x,p.y-30))
    button=widgets['CloseButton'];size=button.slot.get_size();button.slot.set_size(unreal.Vector2D(size.x*.68,size.y*.68))
    assets.set_metadata_tag(bp,'GamePolish20261020','1')
for w in widgets.values():
    if isinstance(w,unreal.Button):
        style=w.get_editor_property('widget_style');style.hovered=style.normal;style.disabled=style.normal;w.set_style(style)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)

# Recharge is clipped by the bomb art's existing alpha, entirely inside the icon.
path='/Game/UI/MiningGUI/Immersive/M_ArtworkOuterRim';protect(path);mat=unreal.load_asset(path)
lib.delete_all_material_expressions(mat)
uv=expression(mat,unreal.MaterialExpressionTextureCoordinate)
art=expression(mat,unreal.MaterialExpressionTextureObjectParameter,parameter_name='Artwork',texture=unreal.load_asset('/Game/UI/MiningGUI/Art/T_CoinBar'))
step=expression(mat,unreal.MaterialExpressionVectorParameter,parameter_name='PixelStep',default_value=unreal.LinearColor(.014,.014,0,0))
side=scalar(mat,'OuterSide',1.)
active=scalar(mat,'RechargeActive',0.)
progress=scalar(mat,'RechargeProgress',0.)
shader=expression(mat,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
pins=[]
for name in ['UV','Artwork','PixelStep','OuterSide','RechargeActive','RechargeProgress']:
    pin=unreal.CustomInput();pin.set_editor_property("input_name",name);pins.append(pin)
shader.set_editor_property("inputs",pins)
for node,pin in [(uv,'UV'),(art,'Artwork'),(step,'PixelStep'),(side,'OuterSide'),(active,'RechargeActive'),(progress,'RechargeProgress')]:connect(node,shader,pin)
shader.set_editor_property('code','''
float4 art=Texture2DSample(Artwork,ArtworkSampler,UV);
float direction=OuterSide>0.5?1:-1;
float2 outside=UV+float2(direction*PixelStep.x,0);
float exterior=any(outside<0)||any(outside>1)?0:Texture2DSample(Artwork,ArtworkSampler,outside).a;
float edge=saturate(art.a-exterior);
float3 color=lerp(art.rgb,float3(1,1,1),edge*0.42);
float filled=step(1-saturate(RechargeProgress),UV.y);
float chargeEdge=(1-smoothstep(0,.018,abs(UV.y-(1-RechargeProgress))))*step(.001,RechargeProgress)*step(RechargeProgress,.999);
float3 charge=lerp(color*.17,float3(.94,.27,.1),filled)+chargeEdge*.6;
return float4(lerp(color,charge,saturate(RechargeActive)),art.a);
''')
lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mask=expression(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True);connect(shader,mask,'')
lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);lib.recompile_material(mat);save(mat)

# A small translucent emissive halo remains visible through the jar's glass in HDR inspection.
path='/Game/CollectionPrototype/Materials/M_CollectionFireflyHalo'
halo=unreal.load_asset(path) or tools.create_asset('M_CollectionFireflyHalo','/Game/CollectionPrototype/Materials',unreal.Material,unreal.MaterialFactoryNew())
lib.delete_all_material_expressions(halo)
halo.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);halo.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
color=expression(halo,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.3,1,.06))
lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
fresnel=expression(halo,unreal.MaterialExpressionFresnel,exponent=1.4,base_reflect_fraction=0.)
inverse=expression(halo,unreal.MaterialExpressionOneMinus);connect(fresnel,inverse,'')
strength=expression(halo,unreal.MaterialExpressionConstant,r=.18)
mult=expression(halo,unreal.MaterialExpressionMultiply);connect(inverse,mult,'A');connect(strength,mult,'B')
lib.connect_material_property(mult,'',unreal.MaterialProperty.MP_OPACITY);lib.recompile_material(halo);save(halo)

path='/Game/StartRoom/UI/M_OpeningTitleDecal';protect(path);decal=unreal.load_asset(path)
sample=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionTextureSample) if n.get_outer()==decal)
fades=[n for n in unreal.ObjectIterator(unreal.MaterialExpressionDecalLifetimeOpacity) if n.get_outer()==decal]
if not fades:
    fade=expression(decal,unreal.MaterialExpressionDecalLifetimeOpacity)
    alpha=expression(decal,unreal.MaterialExpressionMultiply);connect(sample,alpha,'A','A');connect(fade,alpha,'B')
    lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY);lib.recompile_material(decal);save(decal)

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
mapfile=root/'Content/StartRoom/Maps/Lvl_StartRoom.umap'
if not (backup/'Lvl_StartRoom.umap').exists():shutil.copy2(mapfile,backup/'Lvl_StartRoom.umap')
assert levels.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
roomactors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
mapping={'StaticMeshActor_2':'StartRoomView_Robot','SkeletalMeshActor_3':'StartRoomView_Computer','StaticMeshActor_191':'StartRoomView_Book','StaticMeshActor_100':'StartRoomView_Book','StaticMeshActor_129':'StartRoomView_Book'}
scene=[]
for actor in roomactors:
    if actor.get_name() in mapping:
        tag=mapping[actor.get_name()];tags=list(actor.tags)
        if unreal.Name(tag) not in tags:tags.append(unreal.Name(tag));actor.tags=tags
        for primitive in actor.get_components_by_class(unreal.PrimitiveComponent):
            if primitive.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION:
                primitive.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
                primitive.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
            primitive.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY,unreal.CollisionResponseType.ECR_BLOCK)
            primitive.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN,unreal.CollisionResponseType.ECR_IGNORE)
        scene.append({'actor':actor.get_name(),'label':actor.get_actor_label(),'segment':tag})
    if isinstance(actor,unreal.PointLight) and actor.get_actor_label()!='Tunnel_ColdSpill':
        light=actor.point_light_component
        if assets.get_metadata_tag(actor,'GamePolishDimmed')!='1':
            light.set_intensity(light.intensity*.68);assets.set_metadata_tag(actor,'GamePolishDimmed','1')
assert len(scene)==len(mapping),scene
assert levels.save_current_level()
report={'passed':True,'scene_interactions':scene,'main_game_map_saved':False}
(out/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('GAME_POLISH_ASSETS_SAVED')


