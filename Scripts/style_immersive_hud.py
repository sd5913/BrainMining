"""Inset HUD treatment. Backplates are behind contents and never receive input."""
import unreal,json,shutil
from pathlib import Path
root_path=Path(unreal.Paths.project_dir())
backup=root_path/'Saved/Validation/ImmersiveHUD/Before';backup.mkdir(parents=True,exist_ok=True)
for rel in ['UI/MiningGUI/WBP_MiningHUD.uasset','UI/MiningGUI/DrillEnergy/M_DrillEnergy.uasset']:
    dest=backup/Path(rel).name
    if not dest.exists():shutil.copy2(root_path/'Content'/rel,dest)
folder='/Game/UI/MiningGUI/Immersive';lib=unreal.MaterialEditingLibrary
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
material=unreal.load_asset(folder+'/M_InsetBackplate')
if not material:
    material=tools.create_asset('M_InsetBackplate',folder,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=lib.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate)
    side=lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
    side.set_editor_property('parameter_name','OuterSide');side.set_editor_property('default_value',1.)
    opacity=lib.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
    opacity.set_editor_property('parameter_name','BackdropOpacity');opacity.set_editor_property('default_value',.35)
    size=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter)
    size.set_editor_property('parameter_name','PanelSize');size.set_editor_property('default_value',unreal.LinearColor(760,420,0,0))
    shader=lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    shader.set_editor_property('description','Subtle inward taper with a one-pixel outer white rim')
    pins=[]
    for name in ['UV','OuterSide','BackdropOpacity','PanelSize']:
        p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
    shader.set_editor_property('inputs',pins)
    for source,name in [(uv,'UV'),(side,'OuterSide'),(opacity,'BackdropOpacity'),(size,'PanelSize')]:
        assert lib.connect_material_expressions(source,'',shader,name)
    shader.set_editor_property('code','''
float2 size=max(PanelSize.xy,float2(1,1));
float outward=OuterSide>0.5?UV.x:1-UV.x;
float x=outward*size.x;
float y=UV.y*size.y;
float top=(0.012+0.03*(1-outward))*size.y;
float bottom=size.y-top;
float outer=size.x-1-x;
float inside=saturate(min(min(x-1,outer),min(y-top,bottom-y))+0.5);
float rim=1-smoothstep(0.4,1.8,outer);
float cap=(1-smoothstep(0.4,1.4,min(y-top,bottom-y)))*smoothstep(0.82,0.88,outward);
float edge=saturate(max(rim,cap));
float3 rgb=lerp(float3(0.014,0.019,0.022),float3(0.92,0.95,0.96),edge);
float alpha=lerp(BackdropOpacity,0.38,edge)*inside;
return float4(rgb,alpha);
''')
    assert lib.connect_material_property(shader,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mask=lib.create_material_expression(material,unreal.MaterialExpressionComponentMask)
    for key in ['r','g','b']:mask.set_editor_property(key,False)
    mask.set_editor_property('a',True)
    assert lib.connect_material_expressions(shader,'',mask,'')
    assert lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material);assets.save_loaded_asset(material,False)

def plate_material(name,size,side,opacity):
    instance=unreal.load_asset(folder+'/'+name)
    if not instance:
        instance=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance,material)
    lib.set_material_instance_scalar_parameter_value(instance,'OuterSide',side)
    lib.set_material_instance_scalar_parameter_value(instance,'BackdropOpacity',opacity)
    lib.set_material_instance_vector_parameter_value(instance,'PanelSize',unreal.LinearColor(size[0],size[1],0,0))
    assets.save_loaded_asset(instance,False)
    return instance

bp=unreal.load_asset('/Game/UI/MiningGUI/WBP_MiningHUD')
canvas=next(w for w in unreal.ObjectIterator(unreal.CanvasPanel) if w.get_name()=='RootCanvas' and w.get_outer().get_outer()==bp)
tree=canvas.get_outer()
widgets={w.get_name():w for w in unreal.ObjectIterator(unreal.Widget) if w.get_outer()==tree}
if not (backup/'layout.json').exists():
    snapshot={}
    for name in ['CurrencyPanel','AbilityPanel','UpgradePanel','DrillEnergyBar','ReticlePickaxe','ReticleDrill','ReticleBomb']:
        w=widgets[name];s=w.slot;p=s.get_position();z=s.get_size();t=w.get_editor_property('RenderTransform')
        snapshot[name]={'position':[p.x,p.y],'size':[z.x,z.y],'shear':[t.shear.x,t.shear.y],'angle':t.angle}
    (backup/'layout.json').write_text(json.dumps(snapshot,indent=2),encoding='utf8')

def backdrop(parent,name,instance,padding):
    image=widgets.get(name)
    if not image:
        image=unreal.new_object(unreal.Image,outer=tree,name=name)
        parent.add_child_to_canvas(image)
    image.set_brush_from_material(instance)
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    slot=image.slot;slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0),maximum=unreal.Vector2D(1,1)))
    slot.set_alignment(unreal.Vector2D(0,0))
    slot.set_offsets(unreal.Margin(-padding,-padding,-padding,-padding))
    slot.set_z_order(-10)

backdrop(widgets['UpgradePanel'],'UpgradeBackplate',plate_material('MI_UpgradeBackplate',(760,420),1,.42),18)
for name in ['AbilityBackplate','CurrencyBackplate']:
    if name in widgets:
        widgets[name].remove_from_parent()
        widgets[name].set_brush(unreal.SlateBrush())
# Opposite shears point toward the screen centre. Existing slots and springs remain intact.
widgets['CurrencyPanel'].set_render_transform_pivot(unreal.Vector2D(1,0))
widgets['CurrencyPanel'].set_render_shear(unreal.Vector2D(0,-2.5))
widgets['AbilityPanel'].set_render_transform_pivot(unreal.Vector2D(0,1))
widgets['AbilityPanel'].set_render_shear(unreal.Vector2D(0,2.5))
widgets['UpgradePanel'].set_render_shear(unreal.Vector2D(0,-2.))
widgets['DrillEnergyBar'].set_render_transform_pivot(unreal.Vector2D(1,.5))
widgets['DrillEnergyBar'].set_render_shear(unreal.Vector2D(2.5,0))
# White rim derived from each original image's alpha, with no added fill or silhouette.
rim=unreal.load_asset(folder+'/M_ArtworkOuterRim')
if not rim:
    rim=tools.create_asset('M_ArtworkOuterRim',folder,unreal.Material,unreal.MaterialFactoryNew())
    rim.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
    rim.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=lib.create_material_expression(rim,unreal.MaterialExpressionTextureCoordinate)
    artwork=lib.create_material_expression(rim,unreal.MaterialExpressionTextureObjectParameter)
    artwork.set_editor_property('parameter_name','Artwork');artwork.set_editor_property('texture',unreal.load_asset('/Game/UI/MiningGUI/Art/T_CoinBar'))
    step=lib.create_material_expression(rim,unreal.MaterialExpressionVectorParameter)
    step.set_editor_property('parameter_name','PixelStep');step.set_editor_property('default_value',unreal.LinearColor(.014,.014,0,0))
    side=lib.create_material_expression(rim,unreal.MaterialExpressionScalarParameter)
    side.set_editor_property('parameter_name','OuterSide');side.set_editor_property('default_value',1.)
    custom=lib.create_material_expression(rim,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    pins=[]
    for name in ['UV','Artwork','PixelStep','OuterSide']:
        p=unreal.CustomInput();p.set_editor_property('input_name',name);pins.append(p)
    custom.set_editor_property('inputs',pins)
    for source,name in [(uv,'UV'),(artwork,'Artwork'),(step,'PixelStep'),(side,'OuterSide')]:assert lib.connect_material_expressions(source,'',custom,name)
    custom.set_editor_property('code','''
float4 art=Texture2DSample(Artwork,ArtworkSampler,UV);
float direction=OuterSide>0.5?1:-1;
float2 outside=UV+float2(direction*PixelStep.x,0);
float exterior=any(outside<0)||any(outside>1)?0:Texture2DSample(Artwork,ArtworkSampler,outside).a;
float edge=saturate(art.a-exterior);
// Same alpha as the supplied art: brighten its existing outside edge only.
return float4(lerp(art.rgb,float3(1,1,1),edge*0.42),art.a);
''')
    lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mask=lib.create_material_expression(rim,unreal.MaterialExpressionComponentMask)
    for key in ['r','g','b']:mask.set_editor_property(key,False)
    mask.set_editor_property('a',True);assert lib.connect_material_expressions(custom,'',mask,'')
    lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(rim);assets.save_loaded_asset(rim,False)
def original_rim(name,texture,size,side):
    instance=unreal.load_asset(folder+'/'+name) or tools.create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance,rim)
    lib.set_material_instance_texture_parameter_value(instance,'Artwork',texture)
    lib.set_material_instance_scalar_parameter_value(instance,'OuterSide',side)
    lib.set_material_instance_vector_parameter_value(instance,'PixelStep',unreal.LinearColor(1.4/size[0],1.4/size[1],0,0))
    assets.save_loaded_asset(instance,False)
    return instance
coin=widgets['CoinBar'];size=coin.slot.get_size()
coin.set_brush_from_material(original_rim('MI_CurrencyRim',unreal.load_asset('/Game/UI/MiningGUI/Art/T_CoinBar'),(size.x,size.y),1))
for name in ['WBP_AbilityG','WBP_AbilityV']:
    ability=unreal.load_asset('/Game/UI/MiningGUI/'+name)
    before=backup/(name+'.uasset')
    if not before.exists():shutil.copy2(root_path/'Content/UI/MiningGUI'/(name+'.uasset'),before)
    defaults=unreal.get_default_object(ability.generated_class())
    icon=next(w for w in unreal.ObjectIterator(unreal.Image) if w.get_name()=='AbilityIcon' and w.get_outer().get_outer()==ability)
    size=icon.slot.get_size()
    icon.set_brush_from_material(original_rim('MI_'+name+'_Rim',defaults.normal_texture,(size.x,size.y),0))
    unreal.BlueprintEditorLibrary.compile_blueprint(ability);assets.save_loaded_asset(ability,False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp,False)
# The energy rim lives in its own material so it follows the bar's fade and visibility.
energy=unreal.load_asset('/Game/UI/MiningGUI/DrillEnergy/M_DrillEnergy')
node=next(n for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==energy)
code=node.get_editor_property('code')
if 'outerWhiteRim' not in code:
    code=code.replace('return float4(rgb,total);','float outerWhiteRim=smoothstep(0.72,0.88,UV.x)*frame.a*0.24;\nrgb=lerp(rgb,float3(1,1,1),outerWhiteRim);\nreturn float4(rgb,total);')
    node.set_editor_property('code',code);lib.recompile_material(energy);assets.save_loaded_asset(energy,False)
print('IMMERSIVE_HUD_SAVED; only upgrade popup has backing, four upgrade cards supported')
