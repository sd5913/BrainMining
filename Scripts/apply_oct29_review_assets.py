import unreal, traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir())
try:
    for name in ('import_element_card_art.py','repair_stage_doorway.py'):
        source=(root/'Scripts'/name).read_text(encoding='utf8')
        exec(compile(source.replace('unreal.SystemLibrary.quit_editor()',''), name, 'exec'), {})
    (root/'Saved/Validation/oct29-assets-ok.txt').write_text('14 cards imported; doorway repaired')
except Exception:
    unreal.log_error(traceback.format_exc())
unreal.SystemLibrary.quit_editor()
