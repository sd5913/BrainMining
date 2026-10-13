"""Import energy artwork/material and add one editable HUD image."""
import unreal
from pathlib import Path

root = Path(unreal.Paths.project_dir()); folder='/Game/UI/MiningGUI/DrillEnergy'
assets=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools(); lib=unreal.MaterialEditingLibrary
for name in ['T_DrillEnergyFrame','T_DrillEnergyFill']:
    tex=unreal.load_asset(folder+'/'+name)
    if not tex:
        task=unreal.AssetImportTask(); task.filename=str(root/'SourceAssets/DrillEnergy'/(name+'.png'))
        task.destination_path=folder;task.destination_name=name;task.automated=True;task.save=True
        tools.import_asset_tasks([task]);tex=unreal.load_asset(folder+'/'+name)
    tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('never_stream',True);assets.save_loaded_asset(tex,False)
m=unreal.load_asset(folder+'/M_DrillEnergy')
if not m:
    m=tools.create_asset('M_DrillEnergy',folder,unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    energy=lib.create_material_expression(m,unreal.MaterialExpressionScalarParameter)
    energy.set_editor_property('parameter_name','Energy');energy.set_editor_property('default_value',1.)
    shader=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in ['UV','Energy','Frame','Fill']:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);inputs.append(pin)
    shader.set_editor_property('inputs',inputs)
    shader.set_editor_property('code','''
float4 frame=Texture2DSample(Frame,FrameSampler,UV);
float4 fill=Texture2DSample(Fill,FillSampler,UV);
// The artist's fill starts at 5.5%/ends at 93.8% of the rotated height.
float level=saturate((0.955-UV.y)/0.88);
float active=step(level,saturate(Energy))*step(0.0001,Energy);
float a=fill.a*lerp(0.13,0.95,active);
float total=frame.a+a*(1-frame.a);
float3 rgb=(frame.rgb*frame.a+fill.rgb*a*(1-frame.a))/max(total,0.0001);
return float4(rgb,total);
''')
    lib.connect_material_expressions(uv,'',shader,'UV');lib.connect_material_expressions(energy,'',shader,'Energy')
    for name in ['Frame','Fill']:
        tex=lib.create_material_expression(m,unreal.MaterialExpressionTextureObject)
        tex.set_editor_property('texture',unreal.load_asset(folder+'/T_DrillEnergy'+name))
        lib.connect_material_expressions(tex,'',shader,name)
    lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    alpha=lib.create_material_expression(m,unreal.MaterialExpressionComponentMask)
    alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True)
    assert lib.connect_material_expressions(shader,'',alpha,'');lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);assets.save_loaded_asset(m,False)
shader=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==m)
shader.set_editor_property('code','''
float4 frame=Texture2DSample(Frame,FrameSampler,UV);
float4 fill=Texture2DSample(Fill,FillSampler,UV);
float level=saturate((0.955-UV.y)/0.88);
float active=step(level,saturate(Energy))*step(0.0001,Energy);
float a=fill.a*lerp(0.13,0.95,active);
float total=frame.a+a*(1-frame.a);
float3 rgb=(frame.rgb*frame.a+fill.rgb*a*(1-frame.a))/max(total,0.0001);
// Fade to red near empty; the last quarter is fully in the warning color.
float warning=1-smoothstep(0.25,0.30,saturate(Energy));
rgb=lerp(rgb,float3(1.0,0.018,0.028),warning);
float outerWhiteRim=smoothstep(0.72,0.88,UV.x)*frame.a*0.24;
rgb=lerp(rgb,float3(1,1,1),outerWhiteRim);
return float4(rgb,total);
''')
alpha=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionComponentMask) if n.get_outer()==m)
assert lib.connect_material_expressions(shader,'',alpha,'')
lib.recompile_material(m);assets.save_loaded_asset(m,False)
if not globals().get('drill_energy_material_only',False):
    assert unreal.MiningGUIEditorLibrary.add_drill_energy_bar()
    bp=unreal.load_asset('/Game/Showcase/Blueprints/BP_SharedMiningCharacter')
    cdo=unreal.get_default_object(bp.generated_class())
    mining=cdo.get_component_by_class(unreal.MiningComponent)
    assert mining
    mining.set_editor_property('bUseDrillEnergy',True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_loaded_asset(bp,False)
print('DRILL_ENERGY_UI_SAVED; existing weapon poses and reticle slots retained')
