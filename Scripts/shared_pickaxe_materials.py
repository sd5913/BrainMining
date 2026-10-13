"""Shared pickaxe materials. Run in a rendering editor, without -nullrhi."""
import unreal

def ensure_shared_pickaxe_materials():
    lib=unreal.MaterialEditingLibrary
    for name,brightness,wire in [('M_SharedPickaxeBlack',0.,False),('M_SharedPickaxeLines',.65,True)]:
        path='/Game/Passages/Echo/Materials/'+name
        material=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/Passages/Echo/Materials',unreal.Material,unreal.MaterialFactoryNew())
        lib.delete_all_material_expressions(material)
        material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if wire else unreal.BlendMode.BLEND_OPAQUE)
        material.set_editor_property('wireframe',wire)
        color=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant',unreal.LinearColor(brightness,brightness,brightness,1))
        assert lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material)

if __name__=='__main__':ensure_shared_pickaxe_materials()
