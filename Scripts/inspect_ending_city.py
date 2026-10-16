import unreal
from pathlib import Path
root='/Game/Cartoon_City_Free/Meshes/Buildings'
for p in unreal.EditorAssetLibrary.list_assets(root):
    if 'NightLight' in p:continue
    m=unreal.load_asset(p)
    if not isinstance(m,unreal.StaticMesh):continue
    unreal.log('CITY_ASSET '+p+' bounds='+str(m.get_bounds()))
    for slot in m.get_editor_property('static_materials'):
        mat=slot.get_editor_property('material_interface')
        unreal.log('CITY_MATERIAL '+str(mat.get_path_name()))
        if hasattr(unreal.MaterialEditingLibrary,'get_used_textures'):
            unreal.log('CITY_TEXTURES '+str(unreal.MaterialEditingLibrary.get_used_textures(mat)))
texture=unreal.load_asset('/Game/Cartoon_City_Free/Textures/T_Textures')
task=unreal.AssetExportTask();task.object=texture;task.filename=str(Path(unreal.Paths.project_saved_dir(),'city-atlas.png'));task.automated=True;task.prompt=False
unreal.Exporter.run_asset_export_task(task)
