"""Configure the migrated rich-text description without rebuilding the designer layout.

Use MCP ReplaceWidgetWithTemplate (RichTextBlock) and WrapWidgets (ScrollBox)
on ItemDescription first. Only collection UI and pickup subtitle assets are saved.
"""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
out = root/'Saved/Validation/CollectionEmergence'
out.mkdir(parents=True, exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

folder = '/Game/CollectionPrototype/UI'
path = folder+'/DT_CollectionDescriptionStyles'
styles = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if styles is None:
    factory = unreal.DataTableFactory()
    factory.set_editor_property('struct', unreal.RichTextStyleRow.static_struct())
    styles = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DT_CollectionDescriptionStyles', folder, unreal.DataTable, factory)
assert styles
font = unreal.load_asset('/Game/Fonts/F_SourceHanSansCN_Medium')
assert font
rows = []
for name, skew in [('Default', 0.0), ('Quote', 0.18)]:
    rows.append({'Name':name, 'TextStyle':{
        'Font': {'FontObject':font.get_path_name(), 'TypefaceFontName':'Medium', 'Size':17, 'SkewAmount':skew},
        'ColorAndOpacity':{'SpecifiedColor':{'R':0.94,'G':0.93,'B':0.88,'A':1.0}, 'ColorUseRule':'UseColor_Specified'},
        'ShadowOffset':{'X':0.0,'Y':0.0},
    }})
styles.set_editor_property('ignore_missing_fields', True)
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(styles, json.dumps(rows, ensure_ascii=False), unreal.RichTextStyleRow.static_struct())
assert unreal.EditorAssetLibrary.save_loaded_asset(styles, False)

bp = unreal.load_asset(folder+'/WBP_CollectionInspect')
text = unreal.find_object(None, bp.get_path_name()+':WidgetTree.ItemDescription')
assert isinstance(text, unreal.RichTextBlock), 'Migrate ItemDescription through UMG MCP first'
scroll = text.get_parent()
assert isinstance(scroll, unreal.ScrollBox)
scroll.set_visibility(unreal.SlateVisibility.VISIBLE)
scroll.set_consume_mouse_wheel(unreal.ConsumeMouseWheel.WHEN_SCROLLING_POSSIBLE)
slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(scroll)
assert slot
size = slot.get_size()
slot.set_size(unreal.Vector2D(size.x, max(size.y, 300)))
text.set_text_style_set(styles)
text.set_auto_wrap_text(True)
text.set_editor_property('wrap_text_at', max(100, size.x-14))
text.set_editor_property('line_height_percentage', 1.2)
text.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
text.set_text('单独看，每一种试剂都有自己的性质。混合之后，分子之间的相互作用却可能带来新的颜色、气味，甚至一种全新的物质。把简单的东西放在一起，有时会得到超出直觉的结果——试管里似乎藏着更多可能。\n\n<Quote>“加点这个……再加点那个……”</>')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)

script = root/'Scripts/install_collectible_subtitles.py'
exec(compile(script.read_text(encoding='utf-8-sig'), str(script), 'exec'), {})
(out/'assets-updated.json').write_text(json.dumps({'widget':bp.get_path_name(), 'styles':styles.get_path_name(), 'scroll':scroll.get_path_name(), 'description_size':str(slot.get_size()), 'map_saved':False}, indent=2), encoding='utf-8')
unreal.log('COLLECTION_EMERGENCE_ASSETS_UPDATED')
