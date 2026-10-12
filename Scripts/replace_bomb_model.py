"""Replace artist bomb geometry at its existing path and retain material assets."""
import json,shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());backup=root/'Saved/Bomb/BeforeNewModel';backup.mkdir(parents=True,exist_ok=True)
mesh=unreal.load_asset('/Game/Bomb/SM_Bomb');assert mesh
slots=list(mesh.static_materials);origin=mesh.get_bounds().origin
def imported(s):
    try:return str(s.get_editor_property('imported_material_slot_name'))
    except Exception:return str(s.material_slot_name)
old=[{'name':str(s.material_slot_name),'imported':imported(s),'material':s.material_interface.get_path_name() if s.material_interface else None} for s in slots]
assert all(s.material_interface for s in slots),'Existing material missing; preserve for inspection'
shutil.copy2(root/'Content/Bomb/SM_Bomb.uasset',backup/'SM_Bomb.uasset')
source=root/'SourceAssets/Bomb/bomb.fbx'
if source.exists():shutil.copy2(source,backup/'bomb.fbx')
shutil.copy2('C:/Users/Lenovo/Desktop/bomb.fbx',source)
unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
task=unreal.AssetImportTask();task.filename=str(source);task.destination_path='/Game/Bomb';task.destination_name='SM_Bomb';task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.factory=unreal.FbxFactory()
options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;options.original_import_type=unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh=True;options.import_as_skeletal=False;options.import_animations=False;options.import_materials=False;options.import_textures=False
options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=True;options.static_mesh_import_data.import_uniform_scale=1.
options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS;task.options=options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset('/Game/Bomb/SM_Bomb');current=list(mesh.static_materials)
for index,slot in enumerate(current):
    names={str(slot.material_slot_name).casefold(),imported(slot).casefold()}
    match=next((s for s in slots if names & {str(s.material_slot_name).casefold(),imported(s).casefold()}),None)
    # New exports sometimes rename Blender slots; retain section order in that case.
    if match is None:match=slots[min(index,len(slots)-1)]
    slot.material_interface=match.material_interface;current[index]=slot
mesh.set_editor_property('static_materials',current)
assert all(s.material_interface for s in mesh.static_materials)
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,False)
new_origin=mesh.get_bounds().origin;delta=origin-new_origin
if delta.length()>.01:
    for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter','/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter']:
        bp=unreal.load_asset(path)
        if not bp:continue
        cdo=unreal.get_default_object(bp.generated_class())
        model=next(c for c in cdo.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='BombModel')
        model.set_relative_location(model.get_editor_property('relative_location')+delta,False,False)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
report={'old_materials':old,'new_materials':[{'name':str(s.material_slot_name),'material':s.material_interface.get_path_name()} for s in mesh.static_materials],
        'triangles':mesh.get_num_triangles(0),'extent':str(mesh.get_bounds().box_extent),'pivot_compensation':str(delta)}
(root/'Saved/Bomb/model-replacement.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
unreal.log('BOMB_MODEL_REPLACED '+json.dumps(report,ensure_ascii=True))
