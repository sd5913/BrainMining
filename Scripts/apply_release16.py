"""Author the 1.6 glass materials, thicker idle outline and empty gallery floor."""
import json
from pathlib import Path
import unreal

ml=unreal.MaterialEditingLibrary
ea=unreal.EditorAssetLibrary
assets=unreal.AssetToolsHelpers.get_asset_tools()
def node(m,cls,**props):
    n=ml.create_material_expression(m,cls)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def prop(n,p):assert ml.connect_material_property(n,'',p)
def scalar(m,value,p):prop(node(m,unreal.MaterialExpressionConstant,r=value),p)
def material(name):
    path='/Game/MainGame/Materials/'+name
    m=unreal.load_asset(path) if ea.does_asset_exist(path) else assets.create_asset(name,'/Game/MainGame/Materials',unreal.Material,unreal.MaterialFactoryNew())
    ml.delete_all_material_expressions(m)
    return m
def save(m):
    ml.recompile_material(m)
    assert ea.save_loaded_asset(m,False)

m=material('M_ReagentGlass')
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
m.set_editor_property('two_sided',False)
prop(node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.65,.88,.91,1)),unreal.MaterialProperty.MP_BASE_COLOR)
f=node(m,unreal.MaterialExpressionFresnel,exponent=3.,base_reflect_fraction=.04)
mul=node(m,unreal.MaterialExpressionMultiply,const_b=.42)
assert ml.connect_material_expressions(f,'',mul,'A')
add=node(m,unreal.MaterialExpressionAdd,const_b=.045)
assert ml.connect_material_expressions(mul,'',add,'A')
prop(add,unreal.MaterialProperty.MP_OPACITY)
scalar(m,.10,unreal.MaterialProperty.MP_ROUGHNESS)
scalar(m,.8,unreal.MaterialProperty.MP_SPECULAR)
save(m)

m=material('M_ReagentLiquid')
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.03,.6,.4,1))
prop(tint,unreal.MaterialProperty.MP_BASE_COLOR)
scalar(m,.22,unreal.MaterialProperty.MP_ROUGHNESS)
scalar(m,.7,unreal.MaterialProperty.MP_SPECULAR)
save(m)

m=unreal.load_asset('/Game/MainGame/Materials/M_MuseumFocusOutline')
for n in ml.get_material_expressions(m):
    if isinstance(n,unreal.MaterialExpressionCustom):
        code=n.get_editor_property('code').replace('band==0?1.0:','band==0?2.0:').replace('float3(.73,.78,.79),idle*.75','float3(1.0,1.0,1.0),idle')
        code=code.replace('band==1?2.7:3.8','band==1?3.2:4.4').replace('float3(.055,.045,.035),outer*.80','float3(0,0,0),outer').replace('float3(1.0,.95,.85),focus','float3(1.0,1.0,1.0),focus')
        code=code.replace('band==1?3.2:4.4','band==1?3.6:4.8').replace('float3(1.0,1.0,1.0),idle);','float3(1.0,1.0,1.0),idle*.65);')
        n.set_editor_property('code',code)
save(m)

smoke=unreal.load_asset('/Game/WenSi/Museum/Refresh/VFX/NS_MuseumSmoke')
smoke.set_editor_property('warmup_tick_delta',1./30.)
smoke.set_editor_property('warmup_time',5.)
smoke.set_editor_property('warmup_tick_count',150)
assert ea.save_loaded_asset(smoke,False)

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed=[]
for a in actors.get_all_level_actors():
    label=a.get_actor_label()
    if label.startswith('RewardPlinth_') or label.startswith('MM_RewardPlinth_'):
        removed.append({'label':label,'position':str(a.get_actor_location())})
        assert actors.destroy_actor(a)
assert not any(a.get_actor_label().startswith(('RewardPlinth_','MM_RewardPlinth_')) for a in actors.get_all_level_actors())
assert levels.save_current_level()
out=Path(unreal.Paths.project_saved_dir())/'Validation/Release16'
out.mkdir(parents=True,exist_ok=True)
(out/'assets.json').write_text(json.dumps({'removed':removed,'saved':True},indent=2))
unreal.log('RELEASE16_ASSETS_SAVED '+json.dumps(removed))
unreal.SystemLibrary.quit_editor()
