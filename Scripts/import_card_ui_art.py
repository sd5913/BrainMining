import unreal,pathlib
root=pathlib.Path(unreal.Paths.project_dir())
folder='/Game/Rooms/CardDuel/UI'
source=root/'SourceAssets/CardRoomUI'
assets=unreal.AssetToolsHelpers.get_asset_tools(); files=unreal.EditorAssetLibrary
names=['T_Card_Laser','T_Card_Laser_Selected','T_Card_Poison','T_Card_Poison_Selected','T_Card_Fire','T_Card_Fire_Selected','T_Health_Player','T_Health_Boss','T_RoundHeader','T_RoundDots','T_SpeechBubble']
for name in names:
 path=folder+'/'+name
 if files.does_asset_exist(path): files.delete_asset(path)
 task=unreal.AssetImportTask(); task.set_editor_property('filename',str(source/(name+'.png'))); task.set_editor_property('destination_path',folder); task.set_editor_property('destination_name',name); task.set_editor_property('automated',True); task.set_editor_property('save',True); task.set_editor_property('replace_existing',True)
 assets.import_asset_tasks([task])
 tex=unreal.load_asset(path); assert tex,path
 tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI); tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON); tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS); tex.set_editor_property('never_stream',True); tex.set_editor_property('srgb',True); files.save_loaded_asset(tex)
print('CARD_UI_ASSETS_IMPORTED')
