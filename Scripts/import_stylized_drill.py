"""Import baked stylized PBR drill, preserving artist transforms and spin curve."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
spin_curve_before = (root / 'Content/Drill/CF_DrillSpin.uasset').read_bytes()
source = root / 'SourceAssets/Drill/Stylized'
folder = '/Game/Drill/Stylized'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary

def imp(filename, name, options=None):
    existing = unreal.load_asset(folder + '/' + name)
    if existing:
        return existing
    task = unreal.AssetImportTask()
    task.filename = str(source / filename)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
        task.factory = unreal.FbxFactory()
    tools.import_asset_tasks([task])
    asset = unreal.load_asset(folder + '/' + name)
    assert asset, name
    return asset

textures = {}
for channel in ['BaseColor', 'Roughness', 'Metallic', 'Normal']:
    name = 'T_DrillStylized_' + channel
    texture = imp(name + '.png', name)
    texture.set_editor_property('srgb', channel == 'BaseColor')
    compression = (unreal.TextureCompressionSettings.TC_NORMALMAP if channel == 'Normal'
                   else unreal.TextureCompressionSettings.TC_DEFAULT if channel == 'BaseColor'
                   else unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('compression_settings', compression)
    if channel == 'Normal':
        texture.set_editor_property('flip_green_channel', True)
    assert assets.save_loaded_asset(texture, False)
    textures[channel] = texture

material = unreal.load_asset(folder + '/M_DrillStylized')
if not material:
    material = tools.create_asset('M_DrillStylized', folder, unreal.Material, unreal.MaterialFactoryNew())
    for index, (channel, prop) in enumerate([
        ('BaseColor', unreal.MaterialProperty.MP_BASE_COLOR),
        ('Roughness', unreal.MaterialProperty.MP_ROUGHNESS),
        ('Metallic', unreal.MaterialProperty.MP_METALLIC),
        ('Normal', unreal.MaterialProperty.MP_NORMAL),
    ]):
        node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, index * 200)
        node.set_editor_property('parameter_name', channel)
        node.set_editor_property('texture', textures[channel])
        node.set_editor_property('sampler_type',
            unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel == 'Normal' else
            unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel == 'BaseColor' else
            unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        assert lib.connect_material_property(node, 'RGB' if channel in ['BaseColor', 'Normal'] else 'R', prop)
    lib.recompile_material(material)
assert assets.save_loaded_asset(material, False)

unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
opt = unreal.FbxImportUI()
opt.automated_import_should_detect_type = False
opt.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
opt.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
opt.import_mesh = True
opt.import_materials = False
opt.import_textures = False
opt.import_as_skeletal = False
opt.import_animations = False
opt.static_mesh_import_data.combine_meshes = True
opt.static_mesh_import_data.auto_generate_collision = False
opt.static_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
mesh = imp('SM_DrillStylized.fbx', 'SM_DrillStylized', opt)
assert len(mesh.static_materials) == 1
mesh.set_material(0, material)
assert assets.save_loaded_asset(mesh, False)

def pose(component):
    result = {}
    for p in ['relative_location', 'relative_rotation', 'relative_scale3d']:
        value = component.get_editor_property(p)
        fields = ['pitch', 'yaw', 'roll'] if p == 'relative_rotation' else ['x', 'y', 'z']
        result[p] = [round(getattr(value, field), 5) for field in fields]
    return result

players = []
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
subobjects = unreal.SubobjectDataBlueprintFunctionLibrary
for path in ['/Game/Showcase/Blueprints/BP_SharedMiningCharacter',
             '/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter',
             '/Game/ExternalMining/Blueprints/BP_ExternalMiningTestCharacter']:
    bp = unreal.load_asset(path)
    assert bp, path
    components = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        component = subobjects.get_object_for_blueprint(subobjects.get_data(handle), bp)
        if component:
            components[component.get_name()] = component
    before = {name: pose(c) for name, c in components.items() if isinstance(c, unreal.SceneComponent) and name.startswith(('Drill', 'Pickaxe', 'Bomb'))}
    for name in ['DrillRightModel', 'DrillLeftModel']:
        model = components[name]
        model.set_static_mesh(mesh)
        model.set_editor_property('override_materials', [])
    cdo = unreal.get_default_object(bp.generated_class())
    effects = components.get('MiningToolEffects') or cdo.get_component_by_class(unreal.MiningToolEffectsComponent)
    effects.set_editor_property('drill_mesh_asset', mesh)
    if 'BombMotion' in components:
        model, motion = components['BombModel'], components['BombMotion']
        if model.get_attach_parent() != motion:
            model.attach_to_component(motion, 'None', unreal.AttachmentRule.KEEP_RELATIVE,
                                      unreal.AttachmentRule.KEEP_RELATIVE, unreal.AttachmentRule.KEEP_RELATIVE, False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    after = {c.get_name(): pose(c) for c in cdo.get_components_by_class(unreal.SceneComponent) if c.get_name() in before}
    if before != after:
        (root / 'Saved/DrillStylized/pose-difference.json').write_text(json.dumps({'path': path, 'before': before, 'after': after}, indent=2))
    assert before == after, path + ': artist pose changed'
    for name in ['DrillRightModel', 'DrillLeftModel']:
        assert cdo.get_editor_property(name).static_mesh == mesh
    assert assets.save_loaded_asset(bp, False)
    players.append({'blueprint': path, 'poses': after})

curve = root / 'Content/Drill/CF_DrillSpin.uasset'
assert curve.read_bytes() == spin_curve_before
report = {'mesh': mesh.get_path_name(), 'material': material.get_path_name(),
          'triangles': mesh.get_num_triangles(0), 'slots': [str(s.material_slot_name) for s in mesh.static_materials],
          'players': players, 'spin_curve_preserved': True}
(root / 'Saved/DrillStylized/import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('STYLIZED_DRILL_IMPORTED ' + json.dumps(report))
