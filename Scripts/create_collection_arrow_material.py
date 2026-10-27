"""Use the authored arrow alpha with clean UI tint, without its grey RGB fill."""
import unreal
folder='/Game/CollectionPrototype/UI/Redesign'
path=folder+'/M_CollectionArrowTint'
mat=unreal.load_asset(path)
if not mat:
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CollectionArrowTint',folder,unreal.Material,unreal.MaterialFactoryNew())
lib=unreal.MaterialEditingLibrary
lib.delete_all_material_expressions(mat)
mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
sample=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample)
sample.set_editor_property('texture',unreal.load_asset(folder+'/T_CollectionArrow'))
white=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
white.set_editor_property('constant',unreal.LinearColor(1,1,1,1))
assert lib.connect_material_property(white,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(sample,'A',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
unreal.log('COLLECTION_ARROW_TINT_SAVED')
