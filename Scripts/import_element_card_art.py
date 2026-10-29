"""Import the fourteen paper-cut illustrations; share them between UI and table cards."""
import unreal
from pathlib import Path
root = Path(unreal.Paths.project_dir())
base = '/Game/Rooms/CardDuel/UI/Refined'
lib = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
names = 'Lightning Ice Grass Wind Water Earth Metal Light Shadow Star Gravity Time Echo Dragon'.split()
for name in names:
    task = unreal.AssetImportTask()
    for k, v in dict(filename=str(root/'SourceAssets/CardRoom/Elements'/f'{name}.png'), destination_path=base,
                     destination_name='T_Element_'+name, automated=True, replace_existing=True, save=True).items():
        task.set_editor_property(k, v)
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(base+'/T_Element_'+name)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('max_texture_size', 1024)
    assets.save_loaded_asset(texture)
    path = base+'/M_Card_'+name
    m = unreal.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset('M_Card_'+name,base,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(m)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    tex = lib.create_material_expression(m,unreal.MaterialExpressionTextureSample)
    tex.set_editor_property('texture', texture)
    lib.connect_material_property(tex,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    uv = lib.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate)
    mask = lib.create_material_expression(m,unreal.MaterialExpressionCustom)
    mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inp = unreal.CustomInput();inp.set_editor_property('input_name','UV')
    mask.set_editor_property('inputs',[inp])
    mask.set_editor_property('code','float2 q=abs((UV-.5)*float2(1.,1.5))-float2(.44,.69); float d=length(max(q,0))+min(max(q.x,q.y),0)-.06; return saturate(.5-d/max(fwidth(d),.001));')
    lib.connect_material_expressions(uv,'',mask,'UV')
    lib.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(m);assets.save_loaded_asset(m)
unreal.SystemLibrary.quit_editor()
