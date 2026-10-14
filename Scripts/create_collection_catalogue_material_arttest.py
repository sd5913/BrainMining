"""Transparent HDR item render over the catalogue's blurred scene."""
import unreal
def create():
 folder='/Game/CollectionPrototype/Materials';name='M_CollectionCataloguePreview';path=folder+'/'+name
 material=unreal.load_asset(path)
 if material:return
 material=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew());material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 fallback=unreal.load_asset(folder+'/T_CollectionPreviewDefault')
 if not fallback:fallback=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('T_CollectionPreviewDefault',folder,unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
 fallback.set_editor_property('srgb',False);unreal.EditorAssetLibrary.save_loaded_asset(fallback)
 lib=unreal.MaterialEditingLibrary;texture=lib.create_material_expression(material,unreal.MaterialExpressionTextureSampleParameter2D);texture.set_editor_property('parameter_name','Preview');texture.set_editor_property('texture',fallback);texture.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
 color=lib.create_material_expression(material,unreal.MaterialExpressionCustom);color.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);color.set_editor_property('code','float3 c=max(In,0)*8.;return c/(1+c);');inp=unreal.CustomInput();inp.set_editor_property('input_name','In');color.set_editor_property('inputs',[inp]);lib.connect_material_expressions(texture,'RGB',color,'In');lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 alpha=lib.create_material_expression(material,unreal.MaterialExpressionOneMinus);lib.connect_material_expressions(texture,'A',alpha,'');lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
 lib.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material);print('COLLECTION_UI_MATERIAL_READY')
create()
