"""Refresh the jump, restored ink lighting, and direct developer credit screen."""
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
for script in ('apply_ending_white_exit.py', 'apply_ending_scale_zoom.py'):
    exec(compile((project/'Scripts'/script).read_text(encoding='utf-8-sig'), script, 'exec'))
source = (project/'Scripts/apply_ending_paper_finale.py').read_text(encoding='utf-8-sig')
exec(compile(source[source.index("graphic('PaperCaveSurface'"):source.index("graphic('PaperComic'")], 'black_threshold', 'exec'))
credits = unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_DeveloperCredits')
node = unreal.MaterialEditingLibrary.get_material_property_input_node(credits, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert isinstance(node, unreal.MaterialExpressionCustom)
node.set_editor_property('code', 'return Image.rgb*1.25*Power;')
unreal.MaterialEditingLibrary.recompile_material(credits)
assert unreal.EditorAssetLibrary.save_loaded_asset(credits)
unreal.log('ENDING_JUMP_FINISH_READY')
