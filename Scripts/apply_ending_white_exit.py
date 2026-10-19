"""Update only the ending cave pigment and plain white portal/desktop."""
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
source = project / 'Scripts/apply_ending_drill_comic.py'
code = source.read_text(encoding='utf-8-sig').split('# Ending-only tool variant:', 1)[0]
exec(compile(code, str(source), 'exec'))
# Preserve the ending shard fade mask when rebuilding its shared rock material.
finish = (project / 'Scripts/apply_ending_camera_finish.py').read_text(encoding='utf-8-sig')
finish_lib = unreal.MaterialEditingLibrary
exec(compile(finish[finish.index('def fade_mask('):finish.index('for finish_name in')], 'fade_mask', 'exec'))
fade_mask(mat)

source = project / 'Scripts/apply_ending_paper_finale.py'
code = source.read_text(encoding='utf-8-sig')
doodles = [unreal.load_asset('/Engine/EngineResources/DefaultTexture.DefaultTexture')]
exec(compile(code[code.index('def graphic('):code.index("graphic('PaperHorizon'")], str(source), 'exec'))
unreal.log('ENDING_WHITE_EXIT_READY')
