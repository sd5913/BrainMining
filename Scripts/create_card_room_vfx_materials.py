"""Original stylized soft-particle materials for CardRoom; edits only new VFX assets."""
import unreal
ROOT='/Game/Rooms/CardDuel/Materials'
ats=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
for name,additive in [('M_CardRoomFlameSprite',True),('M_CardRoomSmokeSprite',False)]:
    path=ROOT+'/'+name
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else ats.create_asset(name,ROOT,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
    time=lib.create_material_expression(mat,unreal.MaterialExpressionTime)
    mask=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
    ins=[]
    for n in ['UV','Clock']:
        v=unreal.CustomInput();v.set_editor_property('input_name',n);ins.append(v)
    mask.set_editor_property('inputs',ins)
    mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    mask.set_editor_property('code','''float2 p=UV*2-1;
float a=atan2(p.y,p.x); float r=length(p);
float wobble=.065*sin(a*5+Clock*3)+.04*sin(a*9-Clock*4);
float cells=.65+.20*sin((p.x+p.y)*11+Clock*5)+.15*sin((p.x-p.y)*17-Clock*3);
return saturate((1-r+wobble)*4)*saturate(1-r*r)*cells;''')
    lib.connect_material_expressions(uv,'',mask,'UV');lib.connect_material_expressions(time,'',mask,'Clock')
    color=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(r=1,g=.2,b=.03,a=1))
    strength=lib.create_material_expression(mat,unreal.MaterialExpressionConstant);strength.set_editor_property('r',100 if additive else 45)
    glow=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply)
    lib.connect_material_expressions(color,'',glow,'A');lib.connect_material_expressions(strength,'',glow,'B');lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter);opacity.set_editor_property('parameter_name','Opacity');opacity.set_editor_property('default_value',1)
    alpha=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply)
    lib.connect_material_expressions(mask,'',alpha,'A');lib.connect_material_expressions(opacity,'',alpha,'B');lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('CARDROOM_SOFT_VFX_MATERIALS_SAVED')
