"""Use the acquired Planet Project Earth textures in the existing ending.

Run in a clean, dedicated Unreal editor. Only ending materials/map are saved;
the Fab source assets and the main campaign are never edited.
"""
import datetime
import shutil
from pathlib import Path

import unreal

ROOT = '/Game/EndingEmergence'
MAP = ROOT + '/Maps/Lvl_EndingEmergence_Demo'
SOURCE = '/Game/Planet_Project/Assets/Textures/Earth/'
lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
content = Path(unreal.Paths.project_content_dir())

# Only three existing textures are referenced (no source textures are copied).
# Allow headroom for one map backup, shader data and the editor's save temp files.
if shutil.disk_usage(content).free < 3 * 1024**3:
    raise RuntimeError('At least 3 GiB free space is required before changing ending assets.')

if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
    raise RuntimeError('Use a clean dedicated editor; preserve unsaved work.')

textures = {key: unreal.load_asset(SOURCE + name) for key, name in {
    'DayMap': 'T_8k_earth_daymap',
    'NightMap': 'T_8k_earth_nightmap',
    'CloudMap': 'T_8k_earth_clouds',
}.items()}
assert all(textures.values()), 'Add the licensed Planet Project to this project first.'

backup = Path(unreal.Paths.project_saved_dir()) / 'Backups' / ('EndingPlanet-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
for relative in ['Maps/Lvl_EndingEmergence_Demo.umap'] + ['Materials/M_EE_' + n + '.uasset' for n in ('Planet', 'NightContinent', 'CloudDeck', 'EarthAtmosphere')]:
    original = content / 'EndingEmergence' / relative
    if original.exists():
        target = backup / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, target)


def material(name, code, additive=False):
    path = ROOT + '/Materials/M_EE_' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset('M_EE_' + name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_OPAQUE)
    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    bindings = []
    for key, cls, output in [
        ('UV', unreal.MaterialExpressionTextureCoordinate, ''),
        ('N', unreal.MaterialExpressionPixelNormalWS, ''),
        ('V', unreal.MaterialExpressionCameraVectorWS, ''),
        ('T', unreal.MaterialExpressionTime, ''),
        ('P', unreal.MaterialExpressionWorldPosition, 'XYZ'),
    ]:
        bindings.append((key, lib.create_material_expression(mat, cls), output))
    for key, tex in textures.items():
        node = lib.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter)
        node.set_editor_property('parameter_name', key)
        node.set_editor_property('texture', tex)
        node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        bindings.append((key, node, ''))
    inputs = []
    for key, _, _ in bindings:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', key)
        inputs.append(entry)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', code)
    for key, node, output in bindings:
        assert lib.connect_material_expressions(node, output, custom, key)
    assert lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


material('Planet', r'''
float3 n=normalize(N);
// Equirectangular geography independent of the placeholder sphere's UV seam.
// Rotate East Asia toward the existing pullback camera.
float2 q=float2(frac(-atan2(n.y,n.x)/6.2831853+.5-.15),acos(clamp(n.z,-1,1))/3.1415927);
float3 day=Texture2DSample(DayMap,DayMapSampler,q).rgb;
float3 night=Texture2DSample(NightMap,NightMapSampler,q).rgb;
float cloud=Texture2DSample(CloudMap,CloudMapSampler,q+float2(T*.00012,0)).r;
float shadow=Texture2DSample(CloudMap,CloudMapSampler,q+float2(.002+T*.00012,.001)).r;
float sun=dot(n,normalize(float3(.35,-.85,.4)));
float daylight=smoothstep(-.08,.52,sun);
float3 ground=day*(.035+daylight*.85)*(1-shadow*.28);
float3 towns=night*float3(1.25,.91,.68)*2.5*(1-smoothstep(-.18,.22,sun));
float c=saturate(cloud*1.1);
float3 color=(ground+towns)*(1-c*.72)+float3(.64,.76,.92)*c*(.045+daylight*.8);
float rim=pow(1-saturate(dot(n,normalize(V))),4);
return color+float3(.025,.13,.32)*rim*(.25+daylight*.7);
''')

# The continental shot deliberately retains the original procedural coast,
# settlement lights and separate cloud decks from create_ending_emergence.py.

atmosphere = material('EarthAtmosphere', r'''
float3 n=normalize(N);
float limb=pow(1-abs(dot(n,normalize(V))),4.5);
float sun=smoothstep(-.25,.7,dot(n,normalize(float3(.35,-.85,.4))));
return lerp(float3(.008,.035,.11),float3(.035,.23,.58),sun)*limb*.75;
''', additive=True)

assert levels.load_level(MAP)
demo = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.EndingEmergence))
parts = list(demo.get_components_by_class(unreal.StaticMeshComponent))
earth = [c for c in parts if c.get_name().split('_')[0] == 'Earth']
shell = [c for c in parts if c.get_name().split('_')[0] == 'Atmosphere']
lights = [c for c in parts if c.get_name().startswith('EarthLights')]
assert len(earth) == 1 and len(shell) == 1 and len(lights) == 1
# Use the pack's smooth planet silhouette at the existing cinematic size.
planet_mesh = unreal.load_asset('/Game/Planet_Project/Assets/Static_Meshes/SM_Earth_Planet')
assert planet_mesh, 'The acquired planet mesh is required.'
box = planet_mesh.get_bounding_box()
diameter = box.max - box.min
assert min(diameter.x, diameter.y, diameter.z) > 0
assert (box.max + box.min).length() < max(diameter.x, diameter.y, diameter.z) * .01
for component, size in ((earth[0], 8000), (shell[0], 8060)):
    surface = component.get_material(0)
    component.set_static_mesh(planet_mesh)
    component.set_relative_scale3d(unreal.Vector(size / diameter.x, size / diameter.y, size / diameter.z))
    for slot in range(component.get_num_materials()):
        component.set_material(slot, surface)
# Remove the invented land-light positions: the acquired night map supplies them.
lights[0].clear_instances()
shell[0].set_material(0, atmosphere)
assert levels.save_current_level()
unreal.log('ENDING_PLANET_PROJECT_APPLIED: Earth day/night/cloud textures; original route preserved; backup=' + str(backup))
