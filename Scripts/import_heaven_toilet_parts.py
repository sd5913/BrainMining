import unreal,pathlib,json
def import_parts():
 root=pathlib.Path(unreal.Paths.project_dir()).resolve();assets=unreal.AssetToolsHelpers.get_asset_tools();original=unreal.load_asset('/Game/Fab/Toilet/toilet1');material=original.get_editor_property('static_materials')[0].material_interface
 result=[]
 for name in ['SM_HeavenToiletBody','SM_HeavenToiletLid']:
  task=unreal.AssetImportTask();task.filename=str(root/'Art/HeavenPreview/Toilet'/(name+'.fbx'));task.destination_path='/Game/HeavenPreview/Meshes';task.destination_name=name;task.automated=True;task.save=False;task.replace_existing=True
  options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_animations=False;options.set_editor_property('automated_import_should_detect_type',False);options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
  options.static_mesh_import_data.set_editor_property('combine_meshes',True);options.static_mesh_import_data.set_editor_property('import_mesh_lods',False);task.options=options
  unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0');assets.import_asset_tasks([task]);mesh=unreal.load_asset('/Game/HeavenPreview/Meshes/'+name);assert mesh
  slots=mesh.get_editor_property('static_materials')
  for slot in slots:slot.set_editor_property('material_interface',material)
  mesh.set_editor_property('static_materials',slots);unreal.EditorAssetLibrary.save_loaded_asset(mesh)
  result.append({'mesh':mesh.get_path_name(),'bounds':str(mesh.get_bounds()),'material':material.get_path_name()})
 (root/'Saved/HeavenPreview/toilet_parts_import.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print('HEAVEN_PARTS_IMPORTED',json.dumps(result))
import_parts()
