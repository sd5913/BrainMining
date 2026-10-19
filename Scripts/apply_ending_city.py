"""Use acquired Cartoon City Free buildings in the single ending demo only."""
import unreal
import shutil
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("ending_scenery_guard.py")))
from datetime import datetime

ROOT='/Game/EndingEmergence/City'
MAP='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
assets=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
unreal.EditorAssetLibrary.make_directory(ROOT)

def material(name,glass=False):
    p=ROOT+'/'+name
    m=unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else assets.create_asset(name,ROOT,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    lib.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    bindings=[('Color',unreal.MaterialExpressionTextureSample),('N',unreal.MaterialExpressionPixelNormalWS),('P',unreal.MaterialExpressionWorldPosition)]
    inputs=[]
    for name_,_ in bindings:
        i=unreal.CustomInput();i.set_editor_property('input_name',name_);inputs.append(i)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code','''
float light=.5+.5*saturate(dot(normalize(N),normalize(float3(-.7,-.4,1))));
float3 cells=floor(P/float3(120,120,150));
float on=step(.36,frac(sin(dot(cells,float3(12.9898,78.233,39.425)))*43758.5453));
return lerp(float3(.008,.025,.045),float3(.32,.22,.10),on)*light;
''' if glass else '''
float light=.32+.68*saturate(dot(normalize(N),normalize(float3(-.7,-.4,1))));
return (Color.rgb*.48+float3(.025,.035,.050))*light;
''')
    for name_,cls in bindings:
        n=lib.create_material_expression(m,cls)
        if name_=='Color': n.set_editor_property('texture',unreal.load_asset('/Game/Cartoon_City_Free/Textures/T_Textures'))
        assert lib.connect_material_expressions(n,'RGB' if name_=='Color' else '',custom,name_)
    assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m

shell=material('M_CityNightShell');glass=material('M_CityNightWindows',True)
meshes=[]
for name in ['SM_Eco_Building_Grid','SM_Eco_Building_Slope','SM_Eco_Building_Terrace','SM_Regular_Building_TwistedTower_Large']:
    p=ROOT+'/'+name
    source='/Game/Cartoon_City_Free/Meshes/Buildings/'+name
    m=unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else unreal.EditorAssetLibrary.duplicate_asset(source,p)
    assert m
    original=unreal.load_asset(source)
    for i,slot in enumerate(original.get_editor_property('static_materials')):
        mat=slot.get_editor_property('material_interface')
        m.set_material(i,glass if 'Glass' in mat.get_name() else shell)
    assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    meshes.append(m)

props=[]
for folder,name in [('Props','SM_Bus_Stop_02'),('Vegetation','SM_Palm_03'),('Vegetation','SM_Bush_06'),('Props','SM_traffic_light_001'),('Props','SM_Trash_Can_04')]:
    p=ROOT+'/'+name;source='/Game/Cartoon_City_Free/Meshes/'+folder+'/'+name
    mesh=unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else unreal.EditorAssetLibrary.duplicate_asset(source,p)
    assert mesh
    for i,slot in enumerate(mesh.get_editor_property('static_materials')): mesh.set_material(i,shell)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);props.append(mesh)
# The level's fixed night exposure requires a subdued emissive crowd palette.
master=unreal.load_asset('/Game/EndingEmergence/Crowd/Lite/M_CrowdMotion')
color=lib.create_material_expression(master,unreal.MaterialExpressionVertexColor)
scale=lib.create_material_expression(master,unreal.MaterialExpressionMultiply)
scale.set_editor_property('const_b',.16)
assert lib.connect_material_expressions(color,'',scale,'A')
assert lib.connect_material_property(scale,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(master);assert unreal.EditorAssetLibrary.save_loaded_asset(master)

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level(MAP)
demo=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
backup=Path(unreal.Paths.project_saved_dir(),'Backups','EndingCity-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
backup.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(Path(unreal.Paths.project_content_dir(),'EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap'),backup)
demo.set_editor_property('city_building_meshes',meshes);demo.set_editor_property('city_prop_meshes',props);demo.apply_city_buildings()
assert levels.save_current_level()
unreal.log('ENDING_CITY_APPLIED: 196 buildings, 4 acquired silhouettes, opaque unlit night materials; source pack unchanged')
