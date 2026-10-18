"""Create surface-bound title and spherical mineral glow decal materials."""
import unreal,json,sys
from pathlib import Path
lib=unreal.MaterialEditingLibrary;assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
def material(folder,name):
    path=folder+'/'+name
    m=unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    return m
def node(m,cls,**kw):
    n=lib.create_material_expression(m,cls)
    for k,v in kw.items():n.set_editor_property(k,v)
    return n
def output(n,s,p):assert lib.connect_material_property(n,s,p)
def wire(a,s,b,p):assert lib.connect_material_expressions(a,s,b,p)
def custom(m,code,inputs,typ):
    n=node(m,unreal.MaterialExpressionCustom,code=code,output_type=typ)
    entries=[]
    for key in inputs:
        entry=unreal.CustomInput();entry.set_editor_property('input_name',key);entries.append(entry)
    n.set_editor_property('inputs',entries)
    for key,(src,socket) in inputs.items():wire(src,socket,n,key)
    return n
updated=[]
if '--glow-only' not in sys.argv:
    title=material('/Game/StartRoom/UI','M_OpeningTitleDecal')
    tex=node(title,unreal.MaterialExpressionTextureSample,texture=unreal.load_asset('/Game/StartRoom/UI/T_OpeningTitle'))
    output(tex,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    output(tex,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
    output(tex,'A',unreal.MaterialProperty.MP_OPACITY)
    updated.append(title)
glow=material('/Game/MainGame/Materials','M_BuriedMineralGlow')
pos=node(glow,unreal.MaterialExpressionWorldPosition)
center=node(glow,unreal.MaterialExpressionVectorParameter,parameter_name='HintCenter',default_value=unreal.LinearColor(0,0,0,1))
radius=node(glow,unreal.MaterialExpressionScalarParameter,parameter_name='HintRadius',default_value=360.)
amount=node(glow,unreal.MaterialExpressionScalarParameter,parameter_name='HintAmount',default_value=1.)
strength=node(glow,unreal.MaterialExpressionScalarParameter,parameter_name='HintStrength',default_value=.8)
color=node(glow,unreal.MaterialExpressionVectorParameter,parameter_name='HintColor',default_value=unreal.LinearColor(.2,.8,.5,1))
mask=custom(glow,'''
// Keep the authored reach, but concentrate energy instead of filling it with haze.
float d=length(P-Center)/max(Radius,1.0);
float h=1.0-smoothstep(.08,1.0,d);
float core=h*h*h*h;
// World-locked, low-contrast grain keeps the signal attached to the rock.
// No time input: the mineral hint must not shimmer or swim as the camera moves.
float3 q=(P-Center)*.075;
float grain=.72+.28*smoothstep(-.4,.4,sin(q.x+q.z*.41)*sin(q.y-q.z*.63));
return saturate(Amount)*(.62*core+.025*h*h)*grain;
''',{'P':(pos,'XYZ'),'Center':(center,'RGB'),'Radius':(radius,''),'Amount':(amount,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
emission=custom(glow,'return pow(saturate(Color),1.2)*clamp(Strength,0.0,.65);',{'Color':(color,'RGB'),'Strength':(strength,'')},unreal.CustomMaterialOutputType.CMOT_FLOAT3)
output(mask,'',unreal.MaterialProperty.MP_OPACITY);output(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
updated.append(glow)
for m in updated:
    lib.layout_material_expressions(m);lib.recompile_material(m)
    assert assets.save_loaded_asset(m,False)
out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/feedback_materials.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'materials':[m.get_path_name() for m in updated],'glow_style':'compact_core_low_energy_world_locked_grain'},indent=2))
