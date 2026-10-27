import unreal,pathlib,json
root=pathlib.Path(unreal.Paths.project_dir())/'SourceAssets/MuseumRefresh'
folder='/Game/WenSi/Museum/Refresh';assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
def imp(file,name,sub,normal=False,linear=False,force=False):
 path=folder+'/'+sub+'/'+name
 if not force and unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 t=unreal.AssetImportTask();t.filename=str(root/file);t.destination_path=folder+'/'+sub;t.destination_name=name;t.automated=True;t.save=True;t.replace_existing=True
 if file.endswith('.fbx'):
  opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
  opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False
  opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False
  opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS;t.options=opt
 assets.import_asset_tasks([t]);o=unreal.load_asset(path);assert o,path
 if isinstance(o,unreal.Texture):
  o.set_editor_property('srgb',not (normal or linear));o.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if linear else unreal.TextureCompressionSettings.TC_DEFAULT)
  if normal:o.set_editor_property('flip_green_channel',True)
  assert unreal.EditorAssetLibrary.save_loaded_asset(o)
 return o
def material(name,spec,weight=False):
 path=folder+'/Materials/'+name
 m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,folder+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(m,unreal.load_asset('/Game/Museum/Materials/M_PBRMaster'))
 for switch in ['Flat Color','Solid Metalness','Mask Based Roughness','Solid Emissive','Solid Roughness','Mask Based Color','Toggle | World Space']:lib.set_material_instance_static_switch_parameter_value(m,switch,False)
 for p,file in spec:
  tex=imp(file,'T_'+name[3:]+'_'+p,'Textures',normal=p=='Normal',linear=p=='ORM');lib.set_material_instance_texture_parameter_value(m,p,tex)
 for key,value in [('Roughness',1),('Tiling',1),('EmissiveLevel',0)]:lib.set_material_instance_scalar_parameter_value(m,key,value)
 if weight:
  lib.set_material_instance_static_switch_parameter_value(m,'Solid Metalness',True);lib.set_material_instance_scalar_parameter_value(m,'Metalness',0)
  lib.set_material_instance_static_switch_parameter_value(m,'Solid Roughness',True);lib.set_material_instance_scalar_parameter_value(m,'Roughness',.65)
 lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m);return m
m=material('MI_MedievalScale',[('BC','medieval_balance/import/Base.png'),('Normal','medieval_balance/import/Normal.png'),('ORM','medieval_balance/import/ORM.png')])
r={}
for part in ('Stand','Beam','LeftPan','RightPan'):
 o=imp('medieval_balance/MedievalScale'+part+'.fbx','SM_MedievalScale'+part,'Meshes')
 for i in range(len(o.static_materials)):o.set_material(i,m)
 assert unreal.EditorAssetLibrary.save_loaded_asset(o);r[part]=str(o.get_bounds())
m=material('MI_MiningWeight',[('BC','mining_weight/Image_0.png'),('Normal','mining_weight/Image_1.png')],True)
o=imp('mining_weight/MuseumMiningWeight.fbx','SM_MuseumMiningWeight','Meshes',force=True)
for i in range(len(o.static_materials)):o.set_material(i,m)
assert unreal.EditorAssetLibrary.save_loaded_asset(o);r['weight']=str(o.get_bounds())
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumTweaks/scale_import.json').write_text(json.dumps(r,indent=2))
print('MUSEUM_SCALE_IMPORTED',r)
