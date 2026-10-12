"""Import the artist bomb, intact G artwork, and inspect the selected Niagara effect."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = root/'SourceAssets/Bomb'
assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()

def import_asset(filename, name, folder, factory, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.factory = factory
    if options: task.options = options
    tools.import_asset_tasks([task])
    obj = unreal.load_asset(folder+'/'+name)
    assert obj, name
    return obj

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_animations = False
options.import_materials = True
options.import_textures = True
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = True
options.static_mesh_import_data.import_uniform_scale = 1.
options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
mesh = import_asset(source/'bomb.fbx', 'SM_Bomb', '/Game/Bomb', unreal.FbxFactory(), options)
# FBX task saving does not persist all generated material packages in this engine.
for cls in [unreal.Material, unreal.Texture2D]:
    for obj in unreal.ObjectIterator(cls):
        if obj.get_path_name().startswith('/Game/Bomb/'):
            assert assets.save_loaded_asset(obj, False)
texture = import_asset(source/'T_BombAbility.png', 'T_BombAbility', '/Game/UI/MiningGUI/Art', unreal.TextureFactory())
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('never_stream', True)
assert assets.save_loaded_asset(texture, False)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/Realistic_Starter_VFX_Pack_Niagara'], force_rescan=True)
system = unreal.load_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Niagara/Explosion/NS_Explosion_Big_2')
assert system and isinstance(system, unreal.NiagaraSystem)
bounds = mesh.get_bounds()
report = {'mesh': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0),
          'origin': [bounds.origin.x, bounds.origin.y, bounds.origin.z],
          'extent': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
          'materials': [slot.material_interface.get_path_name() if slot.material_interface else None for slot in mesh.static_materials],
          'ui': texture.get_path_name(), 'explosion': system.get_path_name()}
(root/'Saved/Bomb/import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('BOMB_ASSETS '+json.dumps(report))
