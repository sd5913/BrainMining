"""Import museum art without introducing any new gameplay collision."""
import unreal,json,pathlib
root=pathlib.Path(unreal.Paths.project_dir())/'SourceAssets/MuseumRefresh'
folder='/Game/WenSi/Museum/Refresh'
assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
def imp(file,name,sub,normal=False,linear=False):
 path=folder+'/'+sub+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 t=unreal.AssetImportTask();t.filename=str(root/file);t.destination_path=folder+'/'+sub;t.destination_name=name;t.automated=True;t.save=True
 if str(file).lower().endswith('.fbx'):
  opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
  opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False
  opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False
  opt.static_mesh_import_data.remove_degenerates=False
  opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS;t.options=opt
 assets.import_asset_tasks([t]);obj=unreal.load_asset(path);assert obj,path
 if isinstance(obj,unreal.Texture):
  obj.set_editor_property('srgb',not (normal or linear))
  obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if linear else unreal.TextureCompressionSettings.TC_DEFAULT)
  unreal.EditorAssetLibrary.save_loaded_asset(obj)
 return obj
def mi(name,parent,where=None):
 dest=where or folder+'/Materials';path=dest+'/'+name
 m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,dest,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(m,parent);return m
def save(m):
 lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
parent=unreal.load_asset('/Game/Museum/Materials/M_PBRMaster');assert parent
specs={
 'Cat':('concrete_cat_statue/concrete_cat_statue_2k.fbx','concrete_cat_statue/concrete_cat_statue_diff_2k.jpg','concrete_cat_statue/concrete_cat_statue_nor_dx_2k.png','concrete_cat_statue/concrete_cat_statue_ORM.png'),
 'Rock':('rock_09/rock_09_2k.fbx','rock_09/rock_09_diff_2k.jpg','rock_09/rock_09_nor_dx_2k.png','rock_09/rock_09_ORM.png'),
 'Balloon':('balloon/source/Balloon_UE.fbx','balloon/textures/BalloonAlbedo.png','balloon/textures/BalloonNormal.png','balloon/balloon_ORM.png')}
report={}
for name,(model,base,normal,orm) in specs.items():
 mesh=imp(model,'SM_'+name+('_UE' if name=='Balloon' else ''),'Meshes');m=mi('MI_'+name,parent)
 for param,tex in [('BC',imp(base,'T_'+name+'_BaseColor','Textures')),('Normal',imp(normal,'T_'+name+'_Normal','Textures',normal=True)),('ORM',imp(orm,'T_'+name+'_ORM','Textures',linear=True))]:lib.set_material_instance_texture_parameter_value(m,param,tex)
 for switch in ['Flat Color','Solid Metalness','Mask Based Roughness','Solid Emissive','Solid Roughness','Mask Based Color','Toggle | World Space']:lib.set_material_instance_static_switch_parameter_value(m,switch,False)
 lib.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(1,1,1,1))
 lib.set_material_instance_scalar_parameter_value(m,'Roughness',1)
 lib.set_material_instance_scalar_parameter_value(m,'EmissiveLevel',0)
 save(m)
 for i in range(len(mesh.static_materials)):mesh.set_material(i,m)
 unreal.EditorAssetLibrary.save_loaded_asset(mesh);report[name]={'mesh':mesh.get_path_name(),'bounds':str(mesh.get_bounds())}
porcelain=mi('MI_MuseumPorcelain',unreal.load_asset('/Game/WenSi/Museum/Materials/M_MuseumSurface'),'/Game/WenSi/Museum/Materials')
lib.set_material_instance_vector_parameter_value(porcelain,'Tint',unreal.LinearColor(.91,.93,.92,1))
lib.set_material_instance_scalar_parameter_value(porcelain,'Roughness',.17);lib.set_material_instance_scalar_parameter_value(porcelain,'Metallic',0);save(porcelain)
horse=imp('horse_statue_01/horse_statue_01_2k.fbx','SM_PorcelainHorse','Meshes')
for i in range(len(horse.static_materials)):horse.set_material(i,porcelain)
unreal.EditorAssetLibrary.save_loaded_asset(horse);report['Horse']={'mesh':horse.get_path_name(),'bounds':str(horse.get_bounds())}
metal=mi('MI_MuseumMetal032',parent,'/Game/WenSi/Museum/Materials')
for param,file,normal,linear in [('BC','metal/Metal032_2K-JPG_Color.jpg',False,False),('Normal','metal/Metal032_2K-JPG_NormalDX.jpg',True,False),('ORM','metal/metal_ORM.png',False,True)]:
 lib.set_material_instance_texture_parameter_value(metal,param,imp(file,'T_Metal032_'+param,'Textures',normal,linear))
for switch in ['Flat Color','Solid Metalness','Mask Based Roughness','Solid Emissive','Solid Roughness','Mask Based Color','Toggle | World Space']:lib.set_material_instance_static_switch_parameter_value(metal,switch,False)
lib.set_material_instance_vector_parameter_value(metal,'Color',unreal.LinearColor(1,1,1,1));lib.set_material_instance_scalar_parameter_value(metal,'Roughness',1);lib.set_material_instance_scalar_parameter_value(metal,'EmissiveLevel',0);save(metal)
pathlib.Path(unreal.Paths.project_saved_dir(),'Validation/MuseumAssetRefresh/imported.json').write_text(json.dumps(report,indent=2))
print('MUSEUM_REFRESH_IMPORTED',report)
