"""Import the supplied three reticles; retain art exactly and mask its black backdrop."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
lib = unreal.MaterialEditingLibrary
folder = '/Game/UI/MiningGUI/Reticles'
mapping = [('10.png', 'Pickaxe'), ('11.png', 'Drill'), ('12.png', 'Bomb')]
materials = {}
for filename, tool in mapping:
    name = 'T_Reticle' + tool
    texture = unreal.load_asset(folder + '/' + name)
    if not texture:
        task = unreal.AssetImportTask()
        task.filename = str(root / 'SourceAssets/Reticles' / filename)
        task.destination_path = folder
        task.destination_name = name
        task.automated = task.save = True
        tools.import_asset_tasks([task])
        texture = unreal.load_asset(folder + '/' + name)
    assert texture
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    # Coverage-preserving mips keep subpixel strokes visible when the 1080px art
    # is displayed on its small HUD canvas instead of aliasing into broken dots.
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    assert assets.save_loaded_asset(texture, False)
    material = unreal.load_asset(folder + '/M_Reticle' + tool)
    if not material:
        material = tools.create_asset('M_Reticle' + tool, folder, unreal.Material, unreal.MaterialFactoryNew())
        material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
        material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        tex = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
        tex.set_editor_property('parameter_name', 'ReticleTexture')
        tex.set_editor_property('texture', texture)
        tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        # The cross in 10.png is centered 16 source pixels below the canvas center.
        # Correct UVs rather than moving the on-screen aiming point away from center.
        if tool == 'Pickaxe':
            uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -100)
            add = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -650, -100)
            offset = lib.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, -900, 50)
            offset.set_editor_property('r', 0.)
            offset.set_editor_property('g', 16./1080.)
            assert lib.connect_material_expressions(uv, '', add, 'A')
            assert lib.connect_material_expressions(offset, '', add, 'B')
            assert lib.connect_material_expressions(add, '', tex, 'UVs')
        white = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -450, -180)
        white.set_editor_property('constant', unreal.LinearColor(1, 1, 1, 1))
        assert lib.connect_material_property(white, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert lib.connect_material_property(tex, 'R', unreal.MaterialProperty.MP_OPACITY)
        lib.recompile_material(material)
    assert assets.save_loaded_asset(material, False)
    materials[tool] = material

bp = unreal.load_asset('/Game/UI/MiningGUI/WBP_MiningHUD')
canvas = next(o for o in unreal.ObjectIterator(unreal.CanvasPanel)
              if o.get_path_name() == bp.get_path_name() + ':WidgetTree.RootCanvas')
tree = canvas.get_outer()
for _, tool in mapping:
    name = 'Reticle' + tool
    image = next((o for o in unreal.ObjectIterator(unreal.Image)
                  if o.get_path_name() == bp.get_path_name() + ':WidgetTree.' + name), None)
    if not image:
        image = unreal.new_object(unreal.Image, outer=tree, name=name)
        canvas.add_child_to_canvas(image)
        image.slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(.5, .5), maximum=unreal.Vector2D(.5, .5)))
        image.slot.set_alignment(unreal.Vector2D(.5, .5))
        image.slot.set_position(unreal.Vector2D())
        image.slot.set_size(unreal.Vector2D(64, 64))
        image.slot.set_z_order(20)
    image.set_brush_from_material(materials[tool])
    image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if tool == 'Pickaxe' else unreal.SlateVisibility.HIDDEN)

gauge = next(o for o in unreal.ObjectIterator(unreal.MiningGUIJetpackHeatWidget)
             if o.get_path_name() == bp.get_path_name() + ':WidgetTree.JetpackHeatBar')
old_opacity = gauge.warning_max_opacity
gauge.warning_max_opacity = .68
warning = next(o for o in unreal.ObjectIterator(unreal.Image)
               if o.get_path_name() == bp.get_path_name() + ':WidgetTree.JetpackHeatWarning')
assert warning.slot.get_z_order() < 20
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert assets.save_loaded_asset(bp, False)
report = {'reticles': {tool: material.get_path_name() for tool, material in materials.items()},
          'image_size': [64, 64], 'visible_art_size': 'about 34-39 px at 1080p',
          'warning_opacity_before': old_opacity, 'warning_opacity_after': .68,
          'warning_z_order': warning.slot.get_z_order()}
(root / 'Saved/Reticles/import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('TOOL_RETICLES_IMPORTED ' + json.dumps(report))
