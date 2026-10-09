"""UE editor/commandlet entry: import prepared art and create editable UMG blueprints.
Idempotent: existing edited widgets and textures are not overwritten.
"""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
source = root / 'SourceAssets' / 'MiningGUI'
output = root / 'Saved' / 'MiningGUI'
output.mkdir(parents=True, exist_ok=True)
tasks = []
for png in sorted(source.glob('*.png')):
    asset_path = '/Game/UI/MiningGUI/Art/' + png.stem
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        continue
    task = unreal.AssetImportTask()
    task.filename = str(png)
    task.destination_path = '/Game/UI/MiningGUI/Art'
    task.destination_name = png.stem
    task.automated = True
    task.save = False
    task.replace_existing = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    for path in task.imported_object_paths:
        texture = unreal.load_asset(path)
        texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property('never_stream', True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
assert unreal.MiningGUIEditorLibrary.create_mining_gui_assets(), 'Widget Blueprint creation/compile/save failed'
assert unreal.MiningGUIEditorLibrary.repair_missing_gui_fonts(), 'Runtime font asset binding failed'
names = ['WBP_MiningHUD', 'WBP_AbilityG', 'WBP_AbilityV', 'WBP_UpgradePickaxe', 'WBP_UpgradeVortex', 'WBP_UpgradeBlast']
report = {'blueprints': [], 'textures': [], 'passed': True}
for name in names:
    asset = unreal.load_asset('/Game/UI/MiningGUI/' + name)
    assert asset and asset.generated_class(), name
    report['blueprints'].append(asset.get_path_name())
for png in source.glob('*.png'):
    texture = unreal.load_asset('/Game/UI/MiningGUI/Art/' + png.stem)
    assert texture, png.stem
    report['textures'].append(texture.get_path_name())
(output / 'import.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('MINING_GUI_IMPORT ' + json.dumps(report))
