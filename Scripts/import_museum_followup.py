"""Import the CC0 screen, balance parts and bronze shark with existing parents."""
import unreal,json,pathlib
root=pathlib.Path(unreal.Paths.project_dir())/'SourceAssets/MuseumRefresh'
folder='/Game/WenSi/Museum/Refresh';assets=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
def imp(file,name,sub='Meshes',normal=False,linear=False):
 path=folder+'/'+sub+'/'+name
 if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path)
 t=unreal.AssetImportTask();t.filename=str(root/file);t.destination_path=folder+'/'+sub;t.destination_name=name;t.automated=True;t.save=True
 if str(file).lower().endswith('.fbx'):
  opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
  opt.import_mesh=True;opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.import_animations=False
  opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False
  opt.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS;t.options=opt
 assets.import_asset_tasks([t]);obj=unreal.load_asset(path);assert obj,path
 if isinstance(obj,unreal.Texture):
  obj.set_editor_property('srgb',not (normal or linear));obj.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if normal else unreal.TextureCompressionSettings.TC_MASKS if linear else unreal.TextureCompressionSettings.TC_DEFAULT)
  assert unreal.EditorAssetLibrary.save_loaded_asset(obj)
 return obj
def mi(name,parent):
 path=folder+'/Materials/'+name
 m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,folder+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(m,parent);return m
def save(m):
 if isinstance(m,unreal.MaterialInstanceConstant):lib.update_material_instance(m)
 assert unreal.EditorAssetLibrary.save_loaded_asset(m)
surface=unreal.load_asset('/Game/WenSi/Museum/Materials/M_MuseumSurface')
def solid(name,color,rough,metal):
 m=mi(name,surface);lib.set_material_instance_vector_parameter_value(m,'Tint',unreal.LinearColor(*color,1));lib.set_material_instance_scalar_parameter_value(m,'Roughness',rough);lib.set_material_instance_scalar_parameter_value(m,'Metallic',metal);save(m);return m
enamel=solid('MI_ApparatusEnamel',(.15,.21,.19),.42,0)
paper=solid('MI_ScreenPaper',(.72,.67,.51),.83,0)
lacquer=solid('MI_ScreenLacquer',(.085,.026,.018),.29,0)
iron=solid('MI_BalanceIron',(.11,.13,.14),.35,.95)
bronze=solid('MI_BalanceBrass',(.42,.26,.095),.31,.9)
screen=imp('screen/Screen_UE.fbx','SM_FoldingScreen')
screen_mats={'lacquer':lacquer,'paper':paper,'wood':unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Walnut'),'leaf':solid('MI_ScreenLeaves',(.08,.16,.075),.65,0),'blossom':solid('MI_ScreenBlossom',(.43,.21,.18),.65,0),'bronze':bronze}
for i,s in enumerate(screen.static_materials):
 key=str(s.material_slot_name).lower();screen.set_material(i,next((m for n,m in screen_mats.items() if n in key),lacquer))
save(screen)
for name in ['BalanceBeam','BalancePan','BalancePivot']:
 mesh=imp('balance/'+name+'.fbx','SM_'+name)
 for i in range(len(mesh.static_materials)):mesh.set_material(i,bronze if name=='BalancePan' else iron)
 save(mesh)
block=imp('balance/ConductiveBlock.fbx','SM_ConductiveBlock')
for i in range(len(block.static_materials)):block.set_material(i,unreal.load_asset('/Game/WenSi/Museum/Materials/MI_MuseumMetal032'))
save(block)
shark=imp('bronze_shark_statue/bronze_shark_statue_2k.fbx','SM_BronzeShark')
m=mi('MI_BronzeShark',unreal.load_asset('/Game/Museum/Materials/M_PBRMaster'))
for param,file,normal,linear in [('BC','diff_2k.jpg',False,False),('Normal','nor_dx_2k.png',True,False),('ORM','arm_2k.jpg',False,True)]:
 tex=imp('bronze_shark_statue/bronze_shark_statue_'+file,'T_BronzeShark_'+param,'Textures',normal,linear);lib.set_material_instance_texture_parameter_value(m,param,tex)
for switch in ['Flat Color','Solid Metalness','Mask Based Roughness','Solid Emissive','Solid Roughness','Mask Based Color','Toggle | World Space']:lib.set_material_instance_static_switch_parameter_value(m,switch,False)
lib.set_material_instance_vector_parameter_value(m,'Color',unreal.LinearColor(1,1,1,1));lib.set_material_instance_scalar_parameter_value(m,'Roughness',1);lib.set_material_instance_scalar_parameter_value(m,'EmissiveLevel',0);save(m)
for i in range(len(shark.static_materials)):shark.set_material(i,m)
save(shark)
report={n:str(unreal.load_asset(folder+'/Meshes/SM_'+n).get_bounds()) for n in ['FoldingScreen','BalanceBeam','BalancePan','BalancePivot','ConductiveBlock','BronzeShark']}
(pathlib.Path(unreal.Paths.project_saved_dir())/'Validation/MuseumAssetRefresh/Followup/imported.json').write_text(json.dumps(report,indent=2))
print('FOLLOWUP_IMPORTED',report)

