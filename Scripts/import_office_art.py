"""Import office PBR assets without altering gameplay actors or existing Fab assets."""
import unreal,json,pathlib
root=pathlib.Path(unreal.Paths.project_dir());src=root/'Art/Office/Backrooms';folder='/Game/PopupClose/OfficeArt';manifest=json.loads((src/'Manifest.json').read_text(encoding='utf8'))
lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
def imp(file,name,dest,options=None,reimport=False):
 a=unreal.load_asset(dest+'/'+name)
 if a and not reimport:return a
 t=unreal.AssetImportTask();t.filename=str(file);t.destination_path=dest;t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
 if options:t.options=options;t.factory=unreal.FbxFactory()
 tools.import_asset_tasks([t]);a=unreal.load_asset(dest+'/'+name);assert a,name;return a
def constant(m,value,prop):
 n=lib.create_material_expression(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);lib.connect_material_property(n,'',prop)
def color(m,value):
 n=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name','Tint');n.set_editor_property('default_value',unreal.LinearColor(*value));lib.connect_material_property(n,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
mats={}
for key,info in manifest['materials'].items():
 name='M_'+key;m=unreal.load_asset(folder+'/Materials/'+name)
 if not m:
  m=tools.create_asset(name,folder+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
  for role,prop in [('BaseColor',unreal.MaterialProperty.MP_BASE_COLOR),('Normal',unreal.MaterialProperty.MP_NORMAL),('Roughness',unreal.MaterialProperty.MP_ROUGHNESS),('Metallic',unreal.MaterialProperty.MP_METALLIC),('Opacity',unreal.MaterialProperty.MP_OPACITY_MASK)]:
   filename=info['textures'].get(role)
   if not filename:
    if role=='BaseColor':color(m,info['color'])
    elif role in ['Roughness','Metallic']:constant(m,info[role.lower()],prop)
    continue
   tex=imp(src/filename,'T_'+pathlib.Path(filename).stem,folder+'/Textures');tex.set_editor_property('srgb',role=='BaseColor');tex.set_editor_property('max_texture_size',1024)
   tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if role=='Normal' else unreal.TextureCompressionSettings.TC_DEFAULT if role=='BaseColor' else unreal.TextureCompressionSettings.TC_MASKS)
   if role=='Normal':tex.set_editor_property('flip_green_channel',True)
   assets.save_loaded_asset(tex,False)
   n=lib.create_material_expression(m,unreal.MaterialExpressionTextureSampleParameter2D);n.set_editor_property('parameter_name',role);n.set_editor_property('texture',tex);n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
   lib.connect_material_property(n,'RGB' if role in ['Normal','BaseColor'] else 'R',prop)
   if role=='Opacity':m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED);m.set_editor_property('two_sided',True)
  lib.recompile_material(m);assets.save_loaded_asset(m,False)
 mats[key]=m
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=True;opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
summary=[]
for info in manifest['meshes']:
 name=info['name'];mesh=imp(src/(name+'.fbx'),name,folder+'/Meshes',opt,True)
 for i,slot in enumerate(mesh.get_editor_property('static_materials')):
  key=str(slot.get_editor_property('material_slot_name'))
  assert key in mats,(name,key,info['materials'])
  mesh.set_material(i,mats[key])
 assets.save_loaded_asset(mesh,False);b=mesh.get_bounds();summary.append({'name':name,'center':[b.origin.x,b.origin.y,b.origin.z],'size':[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2]})
# World-position grain prevents stretched cube UVs on long walls and carpet tiles.
for name,tint,rough,grain,scale in [('Wall',[.47,.44,.32,1],.86,.035,.08),('Carpet',[.13,.14,.12,1],.98,.07,1.9),('Ceiling',[.52,.51,.43,1],.9,.04,.2),('Trim',[.075,.082,.068,1],.64,0,1),('Blind',[.37,.36,.29,1],.8,.015,.3),('Wood',[.22,.18,.105,1],.7,.035,.03),('Window',[.055,.07,.067,1],.46,0,1),('Paper',[.73,.70,.60,1],.86,0,1),('WarmPanel',[.84,.82,.63,1],.75,0,1)]:
 m=unreal.load_asset(folder+'/Materials/M_BR_'+name)
 if not m:
  m=tools.create_asset('M_BR_'+name,folder+'/Materials',unreal.Material,unreal.MaterialFactoryNew());color(m,tint);constant(m,rough,unreal.MaterialProperty.MP_ROUGHNESS)
  if grain:
   base=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
   noise=lib.create_material_expression(m,unreal.MaterialExpressionNoise);noise.set_editor_property('scale',scale);noise.set_editor_property('levels',2);noise.set_editor_property('output_min',1-grain);noise.set_editor_property('output_max',1+grain)
   mult=lib.create_material_expression(m,unreal.MaterialExpressionMultiply);lib.connect_material_expressions(base,'RGB',mult,'A');lib.connect_material_expressions(noise,'',mult,'B');lib.connect_material_property(mult,'',unreal.MaterialProperty.MP_BASE_COLOR)
  if name=='WarmPanel':
   e=lib.create_material_expression(m,unreal.MaterialExpressionVectorParameter);e.set_editor_property('parameter_name','Glow');e.set_editor_property('default_value',unreal.LinearColor(2.5,2.35,1.8,1));lib.connect_material_property(e,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  lib.recompile_material(m);assets.save_loaded_asset(m,False)
(root/'Saved/OfficeArt/imported_meshes.json').write_text(json.dumps(summary,indent=2),encoding='utf8');print('IMPORTED_OFFICE',len(summary),'meshes',len(mats),'PBR materials')
