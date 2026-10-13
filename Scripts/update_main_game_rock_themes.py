"""Apply four independent PBR stage materials to ordinary terrain only; preserve geometry/cuts."""
import unreal,json,traceback,time,shutil,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());recipe=json.loads((ROOT/'Config/MainGameRockThemes.json').read_text())
folder=recipe['folder'];lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
OUT=ROOT/'Saved/Validation/main_game_rock_themes.json';state={'passed':False,'phase':'starting'};started=time.monotonic()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
def checkpoint(**items):
    state.update(items);OUT.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROCK_THEMES '+json.dumps(items,ensure_ascii=False))
def expression(m,kind,**props):
    e=lib.create_material_expression(m,kind)
    for k,v in props.items():e.set_editor_property(k,v)
    return e
def link(a,out,b,pin):
    names=lib.get_material_expression_input_names(b)
    if pin not in names and len(names)==1:pin=names[0]
    assert lib.connect_material_expressions(a,out,b,pin),(a.get_name(),out,b.get_name(),pin,names)
def parameter(m,name,value,group='Surface'):
    kind=unreal.MaterialExpressionVectorParameter if isinstance(value,list) else unreal.MaterialExpressionScalarParameter
    v=unreal.LinearColor(*value,1) if isinstance(value,list) else value
    return expression(m,kind,parameter_name=name,default_value=v,group=group)
def master():
    path=folder+'/'+recipe['master']
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        existing=unreal.load_asset(path)
        if unreal.EditorAssetLibrary.get_metadata_tag(existing,'MainGameRockThemeOwner')=='FourStages':return existing
        assert unreal.EditorAssetLibrary.delete_asset(path)
    m=unreal.EditorAssetLibrary.duplicate_asset(recipe['source'],path);assert m
    m.set_editor_property('tangent_space_normal',False)
    # Retain the existing museum's lit surface and its Tint/Roughness/Metallic nodes.
    tint=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
    rough=lib.get_material_property_input_node(m,unreal.MaterialProperty.MP_ROUGHNESS)
    assert isinstance(tint,unreal.MaterialExpressionVectorParameter) and isinstance(rough,unreal.MaterialExpressionScalarParameter)
    inputs={'Tint':tint,'Roughness':rough,'P':expression(m,unreal.MaterialExpressionWorldPosition)}
    for name,value in recipe['themes'][0]['scalars'].items():
        if name not in ['Roughness','Metallic','Specular']:inputs[name]=parameter(m,name,value,'Pattern' if 'Scale' in name else 'Surface')
    for name,value in recipe['themes'][0]['vectors'].items():
        if name!='Tint':inputs[name]=parameter(m,name,value,'Palette')
    tint.set_editor_property('group','Palette');rough.set_editor_property('group','Surface')
    custom=expression(m,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
        description='Silica / Dream mineral / Crystal / Organic; UV independent')
    ins=[]
    for name in inputs:
        v=unreal.CustomInput();v.set_editor_property('input_name',name);ins.append(v)
    custom.set_editor_property('inputs',ins)
    outputs=[]
    for name,kind in [('Height',unreal.CustomMaterialOutputType.CMOT_FLOAT1),('Emission',unreal.CustomMaterialOutputType.CMOT_FLOAT3)]:
        o=unreal.CustomOutput();o.set_editor_property('output_name',name);o.set_editor_property('output_type',kind);outputs.append(o)
    custom.set_editor_property('additional_outputs',outputs);custom.set_editor_property('code',(ROOT/'Art/MainGame/Materials/StageRockPattern.hlsl').read_text())
    for name,node in inputs.items():link(node,'XYZ' if name=='P' else 'RGB' if name in ['Tint','MineralLight','Accent'] else '',custom,name)
    rgb=expression(m,unreal.MaterialExpressionComponentMask,r=True,g=True,b=True)
    alpha=expression(m,unreal.MaterialExpressionComponentMask,a=True)
    link(custom,'',rgb,'Input');link(custom,'',alpha,'Input')
    assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_ROUGHNESS)
    assert lib.connect_material_property(custom,'Emission',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    spec=parameter(m,'Specular',.3);assert lib.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    # Reuse the engine's world-space bump function and finite screen derivatives.
    dx=expression(m,unreal.MaterialExpressionDDX);dy=expression(m,unreal.MaterialExpressionDDY)
    link(custom,'Height',dx,'Value');link(custom,'Height',dy,'Value')
    right=expression(m,unreal.MaterialExpressionAdd);down=expression(m,unreal.MaterialExpressionAdd)
    link(custom,'Height',right,'A');link(dx,'',right,'B');link(custom,'Height',down,'A');link(dy,'',down,'B')
    bump=expression(m,unreal.MaterialExpressionMaterialFunctionCall,material_function=unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions03/Procedurals/PerturbNormalHQ'))
    n=expression(m,unreal.MaterialExpressionVertexNormalWS)
    link(custom,'Height',bump,'Bump center');link(right,'',bump,'Bump one pixel right');link(down,'',bump,'Bump one pixel down');link(n,'',bump,'WS Normal')
    assert lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
    lib.layout_material_expressions(m)
    errors=lib.recompile_material(m);assert not errors,errors
    unreal.EditorAssetLibrary.set_metadata_tag(m,'MainGameRockThemeOwner','FourStages')
    assert unreal.EditorAssetLibrary.save_loaded_asset(m);return m
def instance(theme,parent):
    path=folder+'/'+theme['name'];m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(theme['name'],folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(m,parent)
    for name,value in theme['scalars'].items():
        lib.set_material_instance_scalar_parameter_value(m,name,value)
        assert abs(lib.get_material_instance_scalar_parameter_value(m,name)-value)<1e-4,(name,value)
    for name,value in theme['vectors'].items():
        lib.set_material_instance_vector_parameter_value(m,name,unreal.LinearColor(*value,1))
        v=lib.get_material_instance_vector_parameter_value(m,name);assert max(abs(a-b) for a,b in zip([v.r,v.g,v.b],value))<1e-4,name
    lib.update_material_instance(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False);return m
def signature(region):
    rock=region.generated_rock
    return dict(actor=rock.get_path_name(),triangles=rock.triangle_count,cuts=rock.successful_cuts,chunks=len(rock.chunks),
        guide_actors=[g.get_path_name() for g in region.generated_guide_layers],guide_cuts=[g.successful_cuts for g in region.generated_guide_layers],
        guide_materials=[g.rock_material.get_path_name() if g.rock_material else '' for g in region.generated_guide_layers],shell=region.generated_shell.get_path_name())
try:
    backup=ROOT/'Saved/Backups/MainGame_BeforeRockThemes';backup.mkdir(parents=True,exist_ok=True)
    if not (backup/'Lvl_MainGame.umap').exists():shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap')
    protected=['Content/WenSi/Museum/Materials/M_MuseumSurface.uasset','Content/Campaign/Materials/M_CampaignLimestone.uasset','Content/Campaign/Materials/M_CampaignGuideRock.uasset']
    hashes={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in protected}
    checkpoint(phase='creating_materials');parent=master();materials=[instance(t,parent) for t in recipe['themes']]
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    regions=list(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion));assert len(regions)==7
    before={r.get_path_name():signature(r) for r in regions};changes=[]
    for r in regions:
        tags=[str(t) for t in r.tags];index=next((i for i,t in enumerate(recipe['themes']) if t['tag'] in tags),None)
        if index is None:index=next((i for tag,i in recipe['reserves'].items() if tag in tags),None)
        assert index is not None,r.get_actor_label();mat=materials[index];rock=r.generated_rock;assert not rock.guide_material_volumes
        r.set_editor_property('rock_material',mat,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        rock.set_editor_property('rock_material',mat)
        components=rock.get_components_by_class(unreal.DynamicMeshComponent)
        for component in components:component.set_material(0,mat)
        assert signature(r)==before[r.get_path_name()],'Material change modified terrain state'
        changes.append(dict(region=r.get_actor_label(),theme=recipe['themes'][index]['label'],material=mat.get_path_name(),components=len(components),triangles=rock.triangle_count))
    assert levels.save_current_level();assert unreal.EditorAssetLibrary.save_directory(folder,only_if_is_dirty=True,recursive=True)
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==h for p,h in hashes.items())
    checkpoint(passed=True,phase='complete',map='/Game/MainGame/Maps/Lvl_MainGame',saved=True,changes=changes,
        geometry_cuts_guides_and_shells_preserved=True,source_materials_unchanged=True,master=parent.get_path_name(),elapsed_seconds=time.monotonic()-started)
except Exception:checkpoint(phase='failed',error=traceback.format_exc());raise
