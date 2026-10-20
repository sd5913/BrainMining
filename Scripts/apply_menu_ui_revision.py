"""Import the supplied title/fonts and save only UI assets; never rewrites maps."""
from pathlib import Path
import json,unreal
root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/UIRevision';out.mkdir(parents=True,exist_ok=True)
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),'Stop PIE before importing'
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
for source,folder,name in [(root/'SourceAssets/Opening/TitleTransparent.png','/Game/StartRoom/UI','T_OpeningTitle')]+[(root/f'SourceAssets/UI/MenuStyle/T_Menu{n}.png','/Game/UI/MenuStyle',f'T_Menu{n}') for n in ['Panel','Row','Hover','Pressed']]:
    task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True;tools.import_asset_tasks([task])
    texture=unreal.load_asset(folder+'/'+name)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream',True)
    assert assets.save_loaded_asset(texture,False)
assert unreal.ProjectTypographyEditorLibrary.install_credits_fonts()
bp=unreal.load_asset('/Game/CollectionPrototype/UI/WBP_CollectionInspect')
changed=[]
for w in unreal.ObjectIterator(unreal.Widget):
    if not w.get_outer() or w.get_outer().get_outer()!=bp:continue
    if w.get_name()=='PreviewHelp':
        w.set_text('');w.set_visibility(unreal.SlateVisibility.COLLAPSED);changed.append(w.get_name())
    if isinstance(w,unreal.Button):
        style=w.get_editor_property('widget_style');style.hovered=style.normal;style.pressed=style.normal;style.disabled=style.normal
        style.pressed_padding=style.normal_padding;style.hovered_foreground=style.normal_foreground;style.pressed_foreground=style.normal_foreground
        w.set_style(style);changed.append(w.get_name())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp,False)
(out/'assets.json').write_text(json.dumps({'passed':True,'collection_widgets':changed,'fonts':['HiraginoSansGB-W6','Rockwell'],'maps_saved':False},ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('MENU_UI_REVISION_ASSETS_SAVED')
