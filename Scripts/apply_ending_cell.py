"""Import prepared user cell meshes; change only the existing ending demo."""
import unreal, shutil
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).with_name("ending_scenery_guard.py")))
from datetime import datetime
project=Path(unreal.Paths.project_dir()).resolve()
source=project/'SourceAssets/EndingEmergence/Cell'
dest='/Game/EndingEmergence/Cell'
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(dest)
names=['SM_CellHero','SM_CellDistant','SM_CellMitochondria','SM_CellReticulum','SM_CellNucleus']
tasks=[]
for name in names+['T_CellAtlas']:
    if unreal.EditorAssetLibrary.does_asset_exist(dest+'/'+name): continue
    task=unreal.AssetImportTask();task.filename=str(source/(name+('.png' if name.startswith('T_') else '.fbx')))
    task.destination_path=dest;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True
    if name.startswith('SM_'):
        opts=unreal.FbxImportUI();opts.import_mesh=True;opts.import_materials=False;opts.import_textures=False
        opts.import_as_skeletal=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        opts.automated_import_should_detect_type=False
        data=opts.static_mesh_import_data;data.combine_meshes=True;data.auto_generate_collision=False
        data.import_uniform_scale=.01;data.generate_lightmap_u_vs=False
        task.options=opts
    tasks.append(task)
assets.import_asset_tasks(tasks)
for task in tasks:assert task.imported_object_paths,task.filename
tex=unreal.load_asset(dest+'/T_CellAtlas');tex.set_editor_property('max_texture_size',2048)
unreal.EditorAssetLibrary.save_loaded_asset(tex)
def material(name,hero=False):
    path=dest+'/'+name
    m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if hero else unreal.BlendMode.BLEND_OPAQUE)
    lib.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    sample=lib.create_material_expression(m,unreal.MaterialExpressionTextureSample);sample.texture=tex
    normal=lib.create_material_expression(m,unreal.MaterialExpressionPixelNormalWS)
    custom=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    bindings=[]
    for n in ['C','N']:
        i=unreal.CustomInput();i.set_editor_property('input_name',n);bindings.append(i)
    custom.set_editor_property('inputs',bindings)
    custom.set_editor_property('code','float light=.32+.68*saturate(dot(normalize(N),normalize(float3(-.7,-.4,1)))); return (C.rgb*.28+float3(.006,.012,.018))*light;')
    assert lib.connect_material_expressions(sample,'RGB',custom,'C')
    assert lib.connect_material_expressions(normal,'',custom,'N')
    assert lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if hero:
        fade=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
        fade.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        inputs=[]
        for name_,cls in [('Cam',unreal.MaterialExpressionCameraPositionWS),('Center',unreal.MaterialExpressionObjectPositionWS),('Radius',unreal.MaterialExpressionObjectRadius),('P',unreal.MaterialExpressionWorldPosition)]:
            i=unreal.CustomInput();i.set_editor_property('input_name',name_);inputs.append(i)
        fade.set_editor_property('inputs',inputs)
        fade.set_editor_property('code','float a=smoothstep(.35,.8,length(Cam-Center)/max(Radius,1)); float h=frac(sin(dot(floor(P.xy*.06+P.z*.09),float2(12.9898,78.233)))*43758.5453); return step(h,a)*step(.001,a);')
        for name_,cls in [('Cam',unreal.MaterialExpressionCameraPositionWS),('Center',unreal.MaterialExpressionObjectPositionWS),('Radius',unreal.MaterialExpressionObjectRadius),('P',unreal.MaterialExpressionWorldPosition)]:
            n=lib.create_material_expression(m,cls);assert lib.connect_material_expressions(n,'',fade,name_)
        assert lib.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY_MASK)
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
surface=material('M_CellAtlas');hero=material('M_CellAtlasReveal',True)
meshes=[]
for i,name in enumerate(names):
    mesh=unreal.load_asset(dest+'/'+name);assert isinstance(mesh,unreal.StaticMesh)
    mesh.set_material(0,hero if i==0 else surface)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh);meshes.append(mesh)
    unreal.log('CELL_IMPORTED '+name+' '+str(mesh.get_bounds()))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
backup=project/'Saved/Backups'/('EndingCell-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
backup.parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(project/'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap',backup)
demo=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
demo.set_editor_property('cell_visual_meshes',meshes);demo.apply_cell_visuals()
assert levels.save_current_level()
unreal.log('ENDING_CELL_APPLIED: user interior organelles, abstract universe membranes restored; collision and source files preserved')
