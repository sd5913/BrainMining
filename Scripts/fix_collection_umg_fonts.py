"""Persist actual font assets in UMG; transient CoreStyle composite fonts do not serialize."""
import unreal
bp=unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')
font=unreal.load_asset('/Engine/EngineFonts/Roboto.Roboto')
assert font
for name,size in [('ItemName',32),('ItemDescription',17),('PreviewHelp',13)]+[(f'ItemLabel{i}',23) for i in range(4)]+[('PreviousButtonLabel',32),('NextButtonLabel',32)]:
 widget=unreal.find_object(None,bp.get_path_name()+':WidgetTree.'+name)
 assert widget,name
 if widget.get_editor_property('font').font_object:continue
 widget.set_font(unreal.SlateFontInfo(font_object=font,size=size,typeface_font_name='Regular'))
 if name=='PreviousButtonLabel':widget.set_text('‹')
 if name=='NextButtonLabel':widget.set_text('›')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
unreal.log('COLLECTION_UMG_FONTS_SAVED')

