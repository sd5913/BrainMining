import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());dest='/Game/Rooms/CardDuel/Robot'
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
parts=json.loads((root/'SourceAssets/CardRobot/Prepared/parts.json').read_text())
for part in parts:
    task=unreal.AssetImportTask();task.filename=str(root/'SourceAssets/CardRobot/Prepared'/(part['mesh']+'.fbx'));task.destination_path=dest
    task.destination_name=part['mesh'];task.automated=True;task.save=True;task.replace_existing=True
    opt=unreal.FbxImportUI();opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    opt.import_as_skeletal=False;opt.import_materials=False;opt.import_textures=False
    opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False
    task.options=opt;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(dest+'/'+part['mesh']);assert mesh
    if unreal.EditorAssetLibrary.does_asset_exist(dest+'/RobotIvory'):
        materials=[unreal.load_asset(dest+'/RobotIvory'),unreal.load_asset(dest+'/RobotGraphite')]
        for i in range(len(mesh.static_materials)):mesh.set_material(i,materials[1 if part['name'].startswith(('Eye','Brow')) else min(i,1)])
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    part['bounds']=str(mesh.get_bounding_box())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
found=[]
for a in actors:
    if any(x in a.get_actor_label().lower() for x in ['cat','rabbit','clock','opponent']):
        found.append(dict(name=a.get_name(),label=a.get_actor_label(),class_name=a.get_class().get_name(),location=str(a.get_actor_location()),bounds=str(a.get_actor_bounds(False)),scale=str(a.get_actor_scale3d())))
out=root/'Saved/Validation/CardRobot';out.mkdir(parents=True,exist_ok=True)
(out/'import.json').write_text(json.dumps(dict(parts=parts,actors=found),ensure_ascii=False,indent=2),encoding='utf-8')
if not globals().get('CARD_ROBOT_CHAIN',False):unreal.SystemLibrary.quit_editor()
