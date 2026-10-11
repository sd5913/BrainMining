"""Create only /Game/CellUniverse assets. Refuses to overwrite an existing demo map.

Run in a dedicated editor: -ExecutePythonScript=<this file> -unattended.
Materials are code-generated; no downloaded content or external dependencies.
"""
import unreal
from pathlib import Path
import shutil
import datetime

ROOT = '/Game/CellUniverse'
MAP = ROOT + '/Maps/Lvl_CellUniverse_Demo'
assets = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.MaterialEditingLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

rebuild = bool(globals().get('REBUILD_EXISTING', False))
exists = unreal.EditorAssetLibrary.does_asset_exist(MAP)
if exists and not rebuild:
    raise RuntimeError('Demo already exists; refusing to overwrite: ' + MAP)
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
    raise RuntimeError('Use a clean dedicated editor; existing unsaved assets must be preserved.')
if exists:
    backup = Path(unreal.Paths.project_saved_dir()) / 'Backups' / ('CellUniverse-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(unreal.Paths.project_content_dir()) / 'CellUniverse/Maps/Lvl_CellUniverse_Demo.umap', backup / 'Lvl_CellUniverse_Demo.umap')


def material(name, tint, code):
    path = ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    m = assets.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    def node(cls):
        return lib.create_material_expression(m, cls)
    custom = node(unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ['P', 'N', 'UV', 'Tint', 'T', 'HitTime']
    inputs = []
    for name_ in names:
        inp = unreal.CustomInput()
        inp.set_editor_property('input_name', name_)
        inputs.append(inp)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('code', code)
    pos = node(unreal.MaterialExpressionWorldPosition)
    normal = node(unreal.MaterialExpressionPixelNormalWS)
    uv = node(unreal.MaterialExpressionTextureCoordinate)
    color = node(unreal.MaterialExpressionVectorParameter)
    color.set_editor_property('parameter_name', 'Tint')
    color.set_editor_property('default_value', unreal.LinearColor(*tint, 1))
    time = node(unreal.MaterialExpressionTime)
    hit = node(unreal.MaterialExpressionScalarParameter)
    hit.set_editor_property('parameter_name', 'HitTime')
    hit.set_editor_property('default_value', -1000)
    for source, output, target in [(pos, 'XYZ', 'P'), (normal, '', 'N'), (uv, '', 'UV'), (color, '', 'Tint'), (time, '', 'T'), (hit, '', 'HitTime')]:
        lib.connect_material_expressions(source, output, custom, target)
    lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m


surface = '''
float light=.27+.73*saturate(dot(normalize(N),normalize(float3(-.6,-.4,1))));
float grain=.92+.08*sin(P.x*.031)*sin(P.y*.028)*sin(P.z*.023);
return Tint.rgb*light*grain;
'''
material('M_CU_Surface', (.12,.2,.24), surface)
material('M_CU_Glow', (.7,.4,.12), 'return Tint.rgb*2.0;')
stars = material('M_CU_Stars', (.65,.76,1), 'return Tint.rgb*(2.5+.5*sin(T*1.4+P.x*.017));')
stars.set_editor_property('used_with_instanced_static_meshes', True)
lib.recompile_material(stars)
unreal.EditorAssetLibrary.save_loaded_asset(stars)
nebula = material('M_CU_Nebula', (.012,.024,.045), '''
float2 uv=UV*float2(6.283185,3.141593);
float cloud=.3+.7*pow(saturate(.5+.5*sin(uv.x*5+sin(uv.y*7)*2)*sin(uv.y*9)),2);
return Tint.rgb*cloud;
''')
nebula.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
lib.recompile_material(nebula)
unreal.EditorAssetLibrary.save_loaded_asset(nebula)
material('M_CU_Organic', (.32,.04,.09), '''
float fold=sin(P.x*.015+sin(P.z*.012)*2)*sin(P.y*.019+sin(P.z*.021));
float light=.33+.67*saturate(dot(normalize(N),normalize(float3(-.6,-.3,1))));
float ripple=exp(-max(0,T-HitTime)*4)*(.5+.5*sin(length(P.yz)*.06-T*15));
return Tint.rgb*(.65+.35*fold)*light+float3(.18,.09,.08)*ripple;
''')
material('M_CU_Blood', (.7,.02,.05), '''
float light=.24+.76*saturate(dot(normalize(N),normalize(float3(-.7,-.5,.8))));
float center=pow(saturate(abs(N.z)),8);
return Tint.rgb*light*(1-.45*center)+float3(.11,.005,.008);
''')
material('M_CU_Rock', (.13,.075,.23), '''
float veins=abs(sin(P.y*.025+sin(P.z*.012)*3+P.x*.011));
float flecks=pow(saturate(sin(P.y*.21)*sin(P.z*.19)*sin(P.x*.27)),18);
float light=.35+.65*saturate(dot(normalize(N),normalize(float3(-1,-.4,.7))));
return Tint.rgb*(.4+.6*veins)*light+float3(.12,.32,.55)*flecks;
''')
material('M_CU_Membrane', (.07,.34,.3), '''
float2 p=P.yz*.013;
float strand=pow(saturate(.5+.5*sin(p.x*2+sin(p.y*1.7))),14);
float weave=pow(saturate(.5+.5*cos(p.y*2.8+sin(p.x))),20);
float beat=.86+.14*sin(T*1.7+P.y*.004);
float hit=exp(-max(0,T-HitTime)*3.3);
float wave=.5+.5*sin(length(P.yz-float2(0,200))*.045-(T-HitTime)*20);
return Tint.rgb*(.32+strand*.65+weave*.55)*beat+float3(.4,.18,.13)*hit*wave;
''')
material('M_CU_Planet', (.025,.18,.5), '''
float2 q=UV*float2(6.283185,3.141593);
float land=sin(q.x*3+sin(q.y*4)*2)+.6*sin(q.x*7-q.y*5)+.35*sin(q.x*13+q.y*9);
float coast=smoothstep(.18,.35,land);
float ice=smoothstep(.8,.97,abs(cos(q.y)));
float3 c=lerp(Tint.rgb,float3(.12,.38,.20),coast);
c=lerp(c,float3(.7,.83,.85),ice);
float cloud=pow(saturate(.5+.5*sin(q.x*11+q.y*8+sin(q.y*17)*2)),15)*.22;
c=lerp(c,float3(.8,.9,1),cloud);
float light=.18+.82*saturate(dot(normalize(N),normalize(float3(-.7,-.4,.6))));
return c*light;
''')

unreal.EditorAssetLibrary.make_directory(ROOT + '/Maps')
assert (levels.load_level(MAP) if exists else levels.new_level(MAP)), 'Failed to open demo level'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.MiningCellUniverseGameMode)
existing = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.MiningCellUniverse)]
assert len(existing) <= 1, 'Ambiguous demo actors'
demo = existing[0] if existing else actors.spawn_actor_from_class(unreal.MiningCellUniverse, unreal.Vector(0, 0, 0))
demo.set_actor_label('Cell Universe - eleven suspended scale stages')
demo.apply_expanded_preset()
demo.build_demo()
starts = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
start = starts[0] if starts else actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(200, 0, 100), unreal.Rotator(0, 0, 0))
start.set_actor_label('Start inside the cell')
volumes = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.PostProcessVolume)]
exposure = volumes[0] if volumes else actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
exposure.set_editor_property('unbound', True)
settings = exposure.get_editor_property('settings')
for name, value in [('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                    ('auto_exposure_min_brightness', 0.0), ('auto_exposure_max_brightness', 0.0),
                    ('override_auto_exposure_bias', True), ('auto_exposure_bias', 0.0),
                    ('override_bloom_intensity', True), ('bloom_intensity', .35)]:
    settings.set_editor_property(name, value)
exposure.set_editor_property('settings', settings)
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(230, -10, 185), unreal.Rotator(0, 0, 0))
assert len(demo.gates) == 11
assert levels.save_current_level(), 'Could not save demo'
unreal.log('CELL_UNIVERSE_CREATED ' + MAP)
