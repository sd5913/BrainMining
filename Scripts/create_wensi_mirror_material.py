"""Run in Unreal Python; creates only the new mirror material."""
import unreal

path='/Game/WenSi/Mirror/Materials'
name='M_WenSiMirror'
unreal.EditorAssetLibrary.make_directory(path)
if not unreal.EditorAssetLibrary.does_asset_exist(path+'/'+name):
    material=unreal.load_asset(path+'/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,path,unreal.Material,unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    texture=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D)
    texture.set_editor_property('parameter_name','MirrorTexture')
    default_rt=unreal.load_asset(path+'/RT_WenSiMirrorDefault') or unreal.AssetToolsHelpers.get_asset_tools().create_asset('RT_WenSiMirrorDefault',path,unreal.TextureRenderTarget2D,unreal.TextureRenderTargetFactoryNew())
    default_rt.set_editor_property('render_target_format',unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    default_rt.set_editor_property('srgb',False)
    unreal.EditorAssetLibrary.save_loaded_asset(default_rt)
    texture.set_editor_property('texture',default_rt)
    texture.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    uv=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate)
    mask_u=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionComponentMask)
    mask_u.set_editor_property('r',True)
    mask_u.set_editor_property('g',False)
    mask_v=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionComponentMask)
    mask_v.set_editor_property('r',False)
    mask_v.set_editor_property('g',True)
    flip=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionOneMinus)
    append=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionAppendVector)
    unreal.MaterialEditingLibrary.connect_material_expressions(uv,'',mask_u,'')
    unreal.MaterialEditingLibrary.connect_material_expressions(uv,'',mask_v,'')
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_u,'',flip,'')
    unreal.MaterialEditingLibrary.connect_material_expressions(flip,'',append,'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(mask_v,'',append,'B')
    unreal.MaterialEditingLibrary.connect_material_expressions(append,'',texture,'UVs')
    unreal.MaterialEditingLibrary.connect_material_property(texture,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.log('WENSI_MIRROR_MATERIAL_CREATED')
else:
    unreal.log('WENSI_MIRROR_MATERIAL_EXISTS')
