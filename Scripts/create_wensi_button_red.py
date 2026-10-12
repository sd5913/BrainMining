"""Create the red button instance using the museum's existing surface material."""
import unreal
folder='/Game/WenSi/Mirror/Materials'
unreal.EditorAssetLibrary.make_directory(folder)
name='MI_BalanceButtonRed'
material=unreal.load_asset(folder+'/'+name)
if not material:
    material=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
parent=unreal.load_asset('/Game/WenSi/Museum/Materials/MI_Burgundy').get_editor_property('parent')
assert material and parent
names=[str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)]
parameter=next((name for name in ['Tint','BaseColor','Color'] if name in names),None)
if not parameter:
    parent=unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
    names=[str(name) for name in unreal.MaterialEditingLibrary.get_vector_parameter_names(parent)]
    parameter=next((name for name in ['Color','BaseColor','Tint'] if name in names),None)
assert parameter,names
unreal.MaterialEditingLibrary.set_material_instance_parent(material,parent)
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(material,parameter,unreal.LinearColor(.95,.025,.015,1))
# UE 5.8's setter updates the instance but always returns false; verify readback.
color=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material,parameter)
assert abs(color.r-.95)<.001 and abs(color.g-.025)<.001 and abs(color.b-.015)<.001,color
assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
unreal.log('WENSI_RED_BUTTON_MATERIAL_READY')
