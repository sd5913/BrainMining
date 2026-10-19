"""Restore authored desktop effects and select passthrough only for mobile shading.
Restore six authored effects and preserve the Android CelPaint compatibility fix.
"""
import ast,json,shutil,time
from pathlib import Path
import unreal

project=Path(unreal.Paths.project_dir())
lib=unreal.MaterialEditingLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
paths=['/Game/CollectionPrototype/Materials/M_CollectionSilhouette']+[
    '/Game/EndingEmergence/Materials/M_EE_'+n for n in ['Fisheye','ScaleZoom','PulseTransit','WetLens','Exhaustion']]
backup=project/'Saved/Backups'/('DesktopPostEffects_'+time.strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
report={'backup':str(backup),'before':{},'restored':[]}
cel_path='/Game/CelPaint/Main/CelPaint'
for path in paths+[cel_path]:
    m=unreal.load_asset(path);assert m,path
    shutil.copy2(project/('Content/'+path.removeprefix('/Game/')+'.uasset'),backup/(path.rsplit('/',1)[-1]+'.uasset'))
    report['before'][path]=[n.get_editor_property('code') for n in lib.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom)]

def tree(name):return ast.parse((project/'Scripts'/name).read_text(encoding='utf-8-sig'))
def install_function(source,name,environment):
    node=next(n for n in source.body if isinstance(n,ast.FunctionDef) and n.name==name)
    exec(compile(ast.Module(body=[node],type_ignores=[]),'<authored material helper>','exec'),environment)
def literal_calls(source,function):
    return {ast.literal_eval(n.value.args[0]):n.value for n in source.body if isinstance(n,ast.Expr) and isinstance(n.value,ast.Call) and isinstance(n.value.func,ast.Name) and n.value.func.id==function}
def shader_literal(source,needle):
    return next(n.value for n in ast.walk(source) if isinstance(n,ast.Constant) and isinstance(n.value,str) and needle in n.value and 'SceneTextureLookup' in n.value)
def set_shader(path,code):
    m=unreal.load_asset(path)
    nodes=[n for n in lib.get_material_expressions(m) if isinstance(n,unreal.MaterialExpressionCustom)]
    assert len(nodes)==1,(path,len(nodes))
    nodes[0].set_editor_property('code',code.replace('PLATFORM_ANDROID','SHADING_PATH_MOBILE'))
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)

set_shader(paths[0],shader_literal(tree('create_collection_v2.py'),'float center ='))
set_shader(paths[1],shader_literal(tree('create_ending_emergence.py'),'1+.38*r'))
camera=tree('apply_ending_camera_finish.py')
env=dict(unreal=unreal,finish_lib=lib,finish_assets=assets,finish_folder='/Game/EndingEmergence/Materials')
install_function(camera,'post_finish',env)
for name in ['ScaleZoom','PulseTransit']:
    call=literal_calls(camera,'post_finish')[name]
    env['post_finish'](name,ast.literal_eval(call.args[1]),ast.literal_eval(call.args[2]),bridge=name=='ScaleZoom')
surfaces=tree('create_ending_surface_materials.py')
env=dict(unreal=unreal,lib=lib,assets=assets,ROOT='/Game/EndingEmergence')
install_function(surfaces,'make',env)
for name in ['WetLens','Exhaustion']:
    call=literal_calls(surfaces,'make')[name]
    textures={'Drops':unreal.load_asset('/Game/EndingEmergence/Textures/T_EE_DropletsPhotograph')} if name=='WetLens' else {}
    env['make'](name,ast.literal_eval(call.args[1]),textures,ast.literal_eval(call.args[3]),post=True)
for path in paths:
    m=unreal.load_asset(path)
    codes=[]
    for n in lib.get_material_expressions(m):
        if isinstance(n,unreal.MaterialExpressionCustom):
            code=n.get_editor_property('code').replace('PLATFORM_ANDROID','SHADING_PATH_MOBILE')
            n.set_editor_property('code',code);codes.append(code)
    assert any('SceneTextureLookup' in c and '#if SHADING_PATH_MOBILE' in c for c in codes),path
    lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
    report['restored'].append(path)

# Keep the upstream CelPaint Android fix without replacing its desktop shader,
# or stripping the six restored desktop effects on a later Android repair run.
cel=unreal.load_asset(cel_path);assert cel,cel_path
cel_patched=0
for node in lib.get_material_expressions(cel):
    if not isinstance(node,unreal.MaterialExpressionCustom):continue
    code=node.get_editor_property('code')
    if '#if ES3_1_PROFILE' in code or '#if SHADING_PATH_MOBILE' in code:continue
    if 'GetDefaultSceneTextureUV' not in code and 'SceneTextureLookup' not in code:continue
    node.set_editor_property('code','#if SHADING_PATH_MOBILE || ES3_1_PROFILE\nreturn ppinput0;\n#else\n'+code+'\n#endif')
    node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cel_patched+=1
if cel_patched:
    lib.recompile_material(cel);assert unreal.EditorAssetLibrary.save_loaded_asset(cel)
report['celpaint_mobile_nodes_patched']=cel_patched
report['passed']=True
out=project/'Saved/Validation/PackagePlaytest';out.mkdir(parents=True,exist_ok=True)
(out/'desktop-postprocess.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('DESKTOP_POSTPROCESS_RESTORED_WITH_MOBILE_BRANCH')
unreal.SystemLibrary.quit_editor()
