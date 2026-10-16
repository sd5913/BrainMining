"""Refine moss blending on the existing MainGame material family only."""
import unreal,json,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameMoss'
path=OUT/'authoring.json';report=json.loads(path.read_text());report.update(passed=False)
FOLDER='/Game/MainGame/Materials/DirtTransition';MAP='/Game/MainGame/Maps/Lvl_MainGame'
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);lib=unreal.MaterialEditingLibrary
try:
    assert not ed.get_game_world();assert Path(report['backup']).is_dir()
    cfg=json.loads((ROOT/'Config/MainGameTuning.json').read_text())['material']
    assert levels.load_level('/Engine/Maps/Entry')
    master=unreal.load_asset(FOLDER+'/M_DirtTransition')
    custom=next(n for n in lib.get_material_expressions(master) if isinstance(n,unreal.MaterialExpressionCustom))
    code=(ROOT/'Art/MainGame/Materials/DryRockMoss.hlsl').read_text()
    evaluation='\n'.join('if(MudCount>%.1f) proximity=max(proximity,dirt.influence(P,MudStart%d,MudEnd%d,MudReach));'%(i+.5,i,i) for i in range(32))
    custom.set_editor_property('code',code.replace('// __VOLUME_EVALUATION__',evaluation),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    scalars={'MossAmount':cfg['moss_amount'],'MossScale':cfg['moss_scale']}
    vectors={k:unreal.LinearColor(*cfg[c],1) for k,c in [('MossDark','moss_dark'),('MossLight','moss_light')]}
    for n in lib.get_material_expressions(master):
        if not isinstance(n,(unreal.MaterialExpressionScalarParameter,unreal.MaterialExpressionVectorParameter)):continue
        name=str(n.get_editor_property('parameter_name'))
        if name in scalars or name in vectors:n.set_editor_property('default_value',({**scalars,**vectors})[name],notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
    errors=lib.recompile_material(master);assert not errors,list(errors)
    assert unreal.EditorAssetLibrary.save_loaded_asset(master,False)
    changed=[]
    for p in unreal.EditorAssetLibrary.list_assets(FOLDER):
        m=unreal.load_asset(p)
        if not isinstance(m,unreal.MaterialInstanceConstant):continue
        s={str(v.parameter_info.name):v for v in m.get_editor_property('scalar_parameter_values')};v={str(v.parameter_info.name):v for v in m.get_editor_property('vector_parameter_values')}
        for k,x in scalars.items():s[k]=unreal.ScalarParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=x)
        for k,x in vectors.items():v[k]=unreal.VectorParameterValue(parameter_info=unreal.MaterialParameterInfo(name=k),parameter_value=x)
        m.set_editor_property('scalar_parameter_values',list(s.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        m.set_editor_property('vector_parameter_values',list(v.values()),notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False);changed.append(m.get_name())
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,1),(unreal.MaterialProperty.MP_SPECULAR,0),(unreal.MaterialProperty.MP_METALLIC,0),(unreal.MaterialProperty.MP_EMISSIVE_COLOR,0)]:
        node=lib.get_material_property_input_node(master,prop);assert isinstance(node,unreal.MaterialExpressionConstant) and node.get_editor_property('r')==value
    assert levels.load_level(MAP)
    assert hashlib.sha256((ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap').read_bytes()).hexdigest()==report['map_sha256'],'Map content changed'
    report.update(passed=True,saved=True,shader_errors=[],instances=changed,shader_sha256=hashlib.sha256(code.encode()).hexdigest(),map_unchanged=True,error=None)
except Exception:report.update(error=traceback.format_exc());raise
finally:
    path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MOSS_REFINEMENT '+json.dumps(report,ensure_ascii=False))
