"""UE Python commandlet: import only the new, editable drill assets."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = root / 'SourceAssets/Drill'
output = root / 'Saved/Drill'
output.mkdir(parents=True, exist_ok=True)
tools = unreal.AssetToolsHelpers.get_asset_tools()
animation = json.loads((source / 'drill_animation.json').read_text(encoding='utf-8'))

def import_task(filename, name, factory, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = '/Game/Drill'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    task.factory = factory
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    asset = unreal.load_asset('/Game/Drill/' + name)
    assert asset, 'Failed to import ' + name
    return asset

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_animations = False
options.import_materials = False
options.import_textures = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False
options.static_mesh_import_data.import_rotation = unreal.Rotator()
options.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
mesh = import_task(source / 'SM_Drill.fbx', 'SM_Drill', unreal.FbxFactory(), options)

textures = {}
for name, settings in animation['textures'].items():
    role = settings['role']
    texture = import_task(source / settings['file'], name, unreal.TextureFactory())
    texture.srgb = role == 'BaseColor'
    if role == 'Normal':
        texture.compression_settings = unreal.TextureCompressionSettings.TC_NORMALMAP
        texture.flip_green_channel = True
    elif role != 'BaseColor':
        texture.compression_settings = unreal.TextureCompressionSettings.TC_MASKS
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    textures[name] = texture

materials = {}
for settings in animation['materials']:
    material = unreal.load_asset('/Game/Drill/'+settings['name']) or tools.create_asset(settings['name'], '/Game/Drill', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    for index, role in enumerate(['BaseColor','Normal','Roughness','Metallic']):
        prop = {'BaseColor': unreal.MaterialProperty.MP_BASE_COLOR, 'Normal': unreal.MaterialProperty.MP_NORMAL,
                'Roughness': unreal.MaterialProperty.MP_ROUGHNESS, 'Metallic': unreal.MaterialProperty.MP_METALLIC}[role]
        if role in settings['textures']:
            node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -450, index*180)
            node.set_editor_property('parameter_name', role)
            node.texture = textures[settings['textures'][role]]
            node.sampler_type = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            mapping = settings['mapping'].get(role, {'scale':[1,1,1], 'location':[0,0,0]})
            coord = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate,-900,index*180)
            coord.u_tiling, coord.v_tiling = mapping['scale'][:2]
            unreal.MaterialEditingLibrary.connect_material_expressions(coord,'',node,'UVs')
            unreal.MaterialEditingLibrary.connect_material_property(node, 'RGB' if role in ['BaseColor','Normal'] else 'R', prop)
        elif role != 'Normal':
            cls = unreal.MaterialExpressionVectorParameter if role=='BaseColor' else unreal.MaterialExpressionScalarParameter
            node = unreal.MaterialEditingLibrary.create_material_expression(material, cls, -450, index*180)
            node.set_editor_property('parameter_name',role)
            node.set_editor_property('default_value', unreal.LinearColor(*settings['base_color']) if role=='BaseColor' else settings[role.lower()])
            unreal.MaterialEditingLibrary.connect_material_property(node,'',prop)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material,False)
    materials[settings['source_name']] = material
for index, name in enumerate(animation['slot_materials']): mesh.set_material(index,materials[name])
unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)

csv = source / 'CF_DrillSpin.csv'
csv.write_text('\n'.join(f'{time:.9f},{value:.9f}' for time, value in animation['rotation_degrees']), encoding='utf-8')
factory = unreal.CSVImportFactory()
settings = unreal.CSVImportSettings()
settings.import_type = unreal.CSVImportType.ECSV_CURVE_FLOAT
settings.import_curve_interp_mode = unreal.RichCurveInterpMode.RCIM_LINEAR
factory.automated_import_settings = settings
curve = import_task(csv, 'CF_DrillSpin', factory)
assert isinstance(curve, unreal.CurveFloat)
assert abs(curve.get_float_value(animation['duration']) - animation['cycle_delta_degrees']) < 0.01
bounds = mesh.get_bounds()
assert bounds.box_extent.x > max(bounds.box_extent.y, bounds.box_extent.z) * 1.5, 'Drill shaft must point down local X'
report = {'passed': True, 'mesh': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0),
          'extent': [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
          'origin': [bounds.origin.x, bounds.origin.y, bounds.origin.z],
          'materials': [m.get_path_name() for m in materials.values()], 'animation_curve': curve.get_path_name(),
          'source_action': animation['source_action'], 'duration': animation['duration'],
          'textures': [texture.get_path_name() for texture in textures.values()]}
(output / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('DRILL_IMPORTED ' + json.dumps(report))
