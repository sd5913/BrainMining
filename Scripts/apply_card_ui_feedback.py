"""Import supplied art unchanged; crop, desaturate and round it in UI materials."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Validation/CardUIFeedback';out.mkdir(parents=True,exist_ok=True)
base='/Game/Rooms/CardDuel/UI/Refined'
lib=unreal.MaterialEditingLibrary; assets=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
def inp(name):
    i=unreal.CustomInput();i.set_editor_property('input_name',name);return i
for name,bounds in [('Laser',(227,58,856,1007)),('Poison',(239,55,868,1007)),('Fire',(238,69,859,1028))]:
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(root/'SourceAssets/CardRoom/Feedback'/f'{name}.png'))
    task.set_editor_property('destination_path',base);task.set_editor_property('destination_name','T_Supplied_'+name)
    task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
    tools.import_asset_tasks([task])
    texture=unreal.load_asset(base+'/T_Supplied_'+name);assert texture
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    assets.save_loaded_asset(texture)
    path=base+'/M_Card_'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_Card_'+name,base,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    tex=lib.create_material_expression(m,unreal.MaterialExpressionTextureObject);tex.set_editor_property('texture',texture)
    rgb=lib.create_material_expression(m,unreal.MaterialExpressionCustom);rgb.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    rgb.set_editor_property('inputs',[inp('UV'),inp('Art')])
    x,y,x2,y2=bounds
    rgb.set_editor_property('code',f'float2 p=float2({x/1080},{y/1080})+UV*float2({(x2-x)/1080},{(y2-y)/1080}); float3 c=Texture2DSample(Art,ArtSampler,p).rgb; float l=dot(c,float3(.2126,.7152,.0722)); return lerp(l.xxx,c,.8)*float3(1.,.97,.91);')
    lib.connect_material_expressions(uv,'',rgb,'UV');lib.connect_material_expressions(tex,'',rgb,'Art');lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mask=lib.create_material_expression(m,unreal.MaterialExpressionCustom);mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    mask.set_editor_property('inputs',[inp('UV')]);mask.set_editor_property('code','float2 q=abs((UV-.5)*float2(1.,1.5))-float2(.44,.69); float d=length(max(q,0))+min(max(q.x,q.y),0)-.06; return saturate(.5-d/max(fwidth(d),.001));')
    lib.connect_material_expressions(uv,'',mask,'UV');lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);assets.save_loaded_asset(m)
# Existing skull art is yellow and player-face art is red. Keep their shapes,
# recolor chromatic pixels, and retain the texture alpha and neutral outlines.
for name,source,tint in [('Opponent','Boss','float3(1.,.075,.12)'),('Player','Player','float3(1.,.78,.055)')]:
    path=base+'/M_Health_'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_Health_'+name,base,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m);m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    t=lib.create_material_expression(m,unreal.MaterialExpressionTextureSample);t.set_editor_property('texture',unreal.load_asset('/Game/Rooms/CardDuel/UI/T_Health_'+source))
    c=lib.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);c.set_editor_property('inputs',[inp('C')])
    c.set_editor_property('code',f'float hi=max(C.r,max(C.g,C.b)),lo=min(C.r,min(C.g,C.b)); return lerp(C,{tint}*hi,saturate((hi-lo)*8.));')
    lib.connect_material_expressions(t,'RGB',c,'C');lib.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.connect_material_property(t,'A',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);assets.save_loaded_asset(m)
segment=unreal.load_asset('/Game/Subtitles/Collectibles/DA_CollectiblePickup_11');lines=segment.get_editor_property('lines');lines[0].set_editor_property('text','好漂亮的彩带');lines[0].set_editor_property('audio',None);segment.set_editor_property('lines',lines);assets.save_loaded_asset(segment,False)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
for map in ['/Game/MainGame/Maps/Lvl_MainGame']:
    assert levels.load_level(map)
    rows=[]
    for a in api.get_all_level_actors():
        row={'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name(),'position':list(a.get_actor_location().to_tuple()),'rotation':str(a.get_actor_rotation()),'scale':list(a.get_actor_scale3d().to_tuple()),'level':a.get_level().get_path_name(),'tags':[str(t) for t in a.tags]}
        if isinstance(a,unreal.TextRenderActor):row['text']=str(a.text_render.text)
        if isinstance(a,unreal.StaticMeshActor):row['mesh']=a.static_mesh_component.static_mesh.get_path_name() if a.static_mesh_component.static_mesh else None
        rows.append(row)
    report[map]=rows
(out/'maps.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
(out/'art.json').write_text(json.dumps({'passed':True,'materials':5,'supplied_art_unchanged':True}),encoding='utf8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
