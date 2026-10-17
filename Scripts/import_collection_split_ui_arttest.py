"""Use the supplied RGB artwork directly; UI shaders key its flat grey backdrop."""
import unreal,pathlib,datetime,shutil
root=pathlib.Path(unreal.Paths.project_dir());lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools();files=unreal.EditorAssetLibrary
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
def texture(name,folder,source):
 t=unreal.load_asset(folder+'/'+name)
 if not t:
  task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=folder;task.destination_name=name;task.automated=True;task.save=True;assets.import_asset_tasks([task]);t=unreal.load_asset(folder+'/'+name)
 assert t;t.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);t.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS);t.set_editor_property('never_stream',True);files.save_loaded_asset(t);return t
def scalar(m,name,value):
 n=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def shader(m,code,inputs):
 n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);n.set_editor_property('code',code);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 pins=[]
 for name in inputs:
  p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
 n.set_editor_property('inputs',pins)
 for name,node in inputs.items():assert lib.connect_material_expressions(node,'RGBA' if name=='Rect' else '',n,name)
 mask=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask)
 for key,value in [('r',False),('g',False),('b',False),('a',True)]:mask.set_editor_property(key,value)
 assert lib.connect_material_expressions(n,'',mask,'')
 assert lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
 assert lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
def material(name,folder):
 m=unreal.load_asset(folder+'/'+name)
 if m:lib.delete_all_material_expressions(m)
 else:m=assets.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
 m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);return m
folder='/Game/CollectionPrototype/UI'
skins=[('Title',10,(62,391,1750,261)),('Back',11,(666,153,627,705)),('Unknown',12,(125,182,1700,751)),('Chemical',13,(85,154,1750,772))]
textures={name:texture('T_Catalogue'+name,folder,root/'SourceAssets/CollectionCatalogue'/f'{index}.png') for name,index,_ in skins}
m=material('M_CatalogueArtwork',folder);uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
tex=lib.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter);tex.set_editor_property('parameter_name','Artwork');tex.set_editor_property('texture',textures['Title'])
rect=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);rect.set_editor_property('parameter_name','UVRect');rect.set_editor_property('default_value',unreal.LinearColor(0,0,1,1))
shader(m,'''
float2 q=Rect.xy+UV*Rect.zw;
float3 rgb=Texture2DSample(Artwork,ArtworkSampler,q).rgb;
float3 key=Texture2DSample(Artwork,ArtworkSampler,float2(.002,.002)).rgb;
float difference=max(max(abs(rgb.r-key.r),abs(rgb.g-key.g)),abs(rgb.b-key.b));
float a=smoothstep(.002,.012,difference);
if(Blank>.5 && UV.x>.12 && UV.x<.91 && UV.y>.26 && UV.y<.72)
 rgb=Texture2DSample(Artwork,ArtworkSampler,float2(.47,.185)).rgb;
return float4(rgb,a);
''',{'UV':uv,'Artwork':tex,'Rect':rect,'Blank':scalar(m,'Blank',0)})
lib.recompile_material(m);files.save_loaded_asset(m)
for name,index,bounds in skins+[('OwnedBlank',13,(85,154,1750,772))]:
 mi=unreal.load_asset(folder+'/MI_Catalogue'+name)
 if not mi:mi=assets.create_asset('MI_Catalogue'+name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(mi,m);lib.set_material_instance_texture_parameter_value(mi,'Artwork',textures['Chemical' if name=='OwnedBlank' else name])
 x,y,w,h=bounds;lib.set_material_instance_vector_parameter_value(mi,'UVRect',unreal.LinearColor(x/1920,y/1080,w/1920,h/1080));lib.set_material_instance_scalar_parameter_value(mi,'Blank',1 if name=='OwnedBlank' else 0);lib.update_material_instance(mi);files.save_loaded_asset(mi)

folder='/Game/UI/MiningGUI/DrillEnergy';tex=texture('T_DrillEnergyRevised',folder,root/'SourceAssets/DrillEnergy/EnergyRevisedOriginal.png')
backup=root/'Saved/Validation/DrillEnergy'/('BeforeSplitArt_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
shutil.copy2(root/'Content/UI/MiningGUI/DrillEnergy/M_DrillEnergy.uasset',backup/'M_DrillEnergy.uasset')
m=material('M_DrillEnergy',folder);uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
art=lib.create_material_expression(m,unreal.MaterialExpressionTextureObject);art.set_editor_property('texture',tex)
shader(m,'''
// Rotate the supplied horizontal artwork into the existing vertical HUD slot.
float2 pixel=float2(244+(1-UV.y)*1447,324+UV.x*363);
float3 rgb=Texture2DSample(Artwork,ArtworkSampler,pixel/float2(1920,1080)).rgb;
float3 key=Texture2DSample(Artwork,ArtworkSampler,float2(.002,.002)).rgb;
float difference=max(max(abs(rgb.r-key.r),abs(rgb.g-key.g)),abs(rgb.b-key.b));
float a=smoothstep(.002,.012,difference);
float block=step(304.5,pixel.x)*step(pixel.x,1571)*step(389.5,pixel.y)*step(pixel.y,629);
float index=clamp(floor((pixel.x-305)/128),0,9);
float active=step((index+.001)/10,saturate(Energy))*step(.0001,Energy);
float warning=1-smoothstep(.25,.30,saturate(Energy));
// Colour and dim ONLY cells. The cyan frame never receives the warning tint.
rgb=lerp(rgb,float3(1,.018,.028),block*warning);
a*=lerp(1,lerp(.13,.95,active),block);
return float4(rgb,a*.82);
''',{'UV':uv,'Artwork':art,'Energy':scalar(m,'Energy',1)})
lib.recompile_material(m);files.save_loaded_asset(m)
print('SPLIT_CATALOGUE_ART_AND_ENERGY_SAVED')
