"""Create editable contact smoke/spark materials from the user's Niagara pack textures."""
import unreal, json
from pathlib import Path
tools=unreal.AssetToolsHelpers.get_asset_tools()
assets=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
root=Path(unreal.Paths.project_dir())
created=[]
for name, texture_name, smoke in [('M_DrillSmoke','T_Smoke_Wisp',True),('M_DrillSparks','T_Spark_1',False)]:
    material=unreal.load_asset('/Game/Drill/'+name) or tools.create_asset(name,'/Game/Drill',unreal.Material,unreal.MaterialFactoryNew())
    texture=unreal.load_asset('/Game/Realistic_Starter_VFX_Pack_Niagara/Textures/'+texture_name)
    assert texture
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if smoke else unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property('two_sided',True)
    material.set_editor_property('used_with_instanced_static_meshes',True)
    if smoke: material.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    else: material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    def node(cls,x,y,**props):
        n=unreal.MaterialEditingLibrary.create_material_expression(material,cls,x,y)
        for key,value in props.items(): n.set_editor_property(key,value)
        return n
    def wire(a,out,b,input):
        assert unreal.MaterialEditingLibrary.connect_material_expressions(a,out,b,input)
    def output(n,out,prop):
        assert unreal.MaterialEditingLibrary.connect_material_property(n,out,prop)
    mask=node(unreal.MaterialExpressionTextureSampleParameter2D,-700,0,parameter_name='Flipbook' if smoke else 'SparkTexture',texture=texture)
    if smoke:
        uv=node(unreal.MaterialExpressionTextureCoordinate,-1700,-150)
        scale=node(unreal.MaterialExpressionMultiply,-1500,-150,const_b=.125)
        wire(uv,'',scale,'A')
        u=node(unreal.MaterialExpressionPerInstanceCustomData,-1700,-400,data_index=1)
        v=node(unreal.MaterialExpressionPerInstanceCustomData,-1700,-550,data_index=2)
        append=node(unreal.MaterialExpressionAppendVector,-1500,-400)
        wire(u,'',append,'A');wire(v,'',append,'B')
        pixel=node(unreal.MaterialExpressionVertexInterpolator,-1300,-400)
        wire(append,'',pixel,'VS')
        offset=node(unreal.MaterialExpressionAdd,-1000,-150)
        wire(scale,'',offset,'A');wire(pixel,'',offset,'B');wire(offset,'',mask,'UVs')
    alpha=node(unreal.MaterialExpressionPerInstanceCustomData,-700,250,data_index=0)
    alpha_pixel=node(unreal.MaterialExpressionVertexInterpolator,-500,250)
    wire(alpha,'',alpha_pixel,'VS')
    opacity=node(unreal.MaterialExpressionMultiply,-300,50)
    if smoke:
        gain=node(unreal.MaterialExpressionScalarParameter,-700,400,parameter_name='SmokeOpacityGain',default_value=2.8)
        mask_gain=node(unreal.MaterialExpressionMultiply,-500,50)
        wire(mask,'A',mask_gain,'A');wire(gain,'',mask_gain,'B');wire(mask_gain,'',opacity,'A')
    else: wire(mask,'R',opacity,'A')
    wire(alpha_pixel,'',opacity,'B')
    if smoke:
        fade=node(unreal.MaterialExpressionDepthFade,-50,50,fade_distance_default=24)
        wire(opacity,'',fade,'Opacity');output(fade,'',unreal.MaterialProperty.MP_OPACITY)
        tint=node(unreal.MaterialExpressionVectorParameter,-500,-500,parameter_name='SoilTint',default_value=unreal.LinearColor(.22,.18,.12,1))
        output(tint,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
        # Small ambient fill keeps dust visible in a dark tunnel without a glowing cloud.
        ambient=node(unreal.MaterialExpressionMultiply,-150,-400,const_b=.2)
        wire(tint,'RGB',ambient,'A');output(ambient,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        tint=node(unreal.MaterialExpressionVectorParameter,-500,-400,parameter_name='SparkTint',default_value=unreal.LinearColor(1,.31,.045,1))
        intensity=node(unreal.MaterialExpressionScalarParameter,-500,-250,parameter_name='Intensity',default_value=8)
        energy=node(unreal.MaterialExpressionMultiply,-200,-300)
        wire(tint,'RGB',energy,'A');wire(intensity,'',energy,'B');output(energy,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        output(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    assert assets.save_loaded_asset(material,False)
    created.append(material.get_path_name())

# Improve visibility of the existing soft dust layer using ambient fill as well.
dust=unreal.load_asset('/Game/Drill/M_DrillDust')
fill=unreal.MaterialEditingLibrary.create_material_expression(dust,unreal.MaterialExpressionVectorParameter,-250,-700)
fill.set_editor_property('parameter_name','AmbientDustFill')
fill.set_editor_property('default_value',unreal.LinearColor(.065,.05,.033,1))
unreal.MaterialEditingLibrary.connect_material_property(fill,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
unreal.MaterialEditingLibrary.recompile_material(dust)
assert assets.save_loaded_asset(dust,False)
assert unreal.MiningGUIEditorLibrary.add_drill_cooldown_overlay()

# Change only drill presentation on the user's current character blueprint.
bp=unreal.load_asset('/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter')
cdo=unreal.get_default_object(bp.generated_class())
effects=cdo.get_component_by_class(unreal.MiningToolEffectsComponent)
effects.drill_location=unreal.Vector(64,25,-10)
effects.drill_rotation=unreal.Rotator(pitch=14,yaw=-18,roll=0)
effects.drill_scale=.72
effects.set_editor_property('bDualDrills',True)
effects.left_drill_location=unreal.Vector(64,-25,-10)
effects.left_drill_rotation=unreal.Rotator(pitch=14,yaw=18,roll=0)
assert assets.save_loaded_asset(bp,False)
(root/'Saved/Drill/contact-assets.json').write_text(json.dumps({'passed':True,'materials':created,'cooldown_widget':'WBP_AbilityV.CooldownText','pose':[64,25,-10]},indent=2))
unreal.log('DRILL_CONTACT_ASSETS_READY')
