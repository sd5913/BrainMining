"""Import the supplied menu/wall logos without changing their pixels or maps."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir())
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
textures = {}
for source, name in [('TitleTransparent.png', 'T_OpeningTitle'),
                     ('TitleWall.png', 'T_OpeningWallTitle')]:
    task = unreal.AssetImportTask()
    task.filename = str(root / 'SourceAssets/Opening' / source)
    task.destination_path = '/Game/StartRoom/UI'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset('/Game/StartRoom/UI/' + name)
    assert texture
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('never_stream', True)
    texture.set_editor_property('srgb', True)
    assert assets.save_loaded_asset(texture, False)
    textures[name] = texture

# Keep the surface projection and fade, with restrained emission so the cream
# lettering retains its color instead of washing out to white under room lights.
decal = unreal.load_asset('/Game/StartRoom/UI/M_OpeningTitleDecal')
samples = [n for n in materials.get_material_expressions(decal)
           if isinstance(n, unreal.MaterialExpressionTextureSample)]
assert len(samples) == 1, 'Expected exactly one wall logo sample'
samples[0].set_editor_property('texture', textures['T_OpeningWallTitle'])
emission = [n for n in materials.get_material_expressions(decal)
            if isinstance(n, unreal.MaterialExpressionCustom)
            and 'RGB*Alpha*saturate(Fade)' in n.get_editor_property('code')]
assert len(emission) == 1, 'Expected the alpha-masked wall emission'
emission[0].set_editor_property('code', 'return RGB*Alpha*saturate(Fade)*0.25;')
materials.recompile_material(decal)
assert assets.save_loaded_asset(decal, False)

report = root / 'Saved/Validation/OpeningLogos'
report.mkdir(parents=True, exist_ok=True)
(report / 'import.json').write_text(json.dumps({
    'passed': True, 'textures': {k: v.get_path_name() for k, v in textures.items()},
    'wall_material': decal.get_path_name(), 'wall_emission_scale': 0.25,
    'maps_saved': False,
}, indent=2), encoding='utf-8')
unreal.SystemLibrary.quit_editor()
