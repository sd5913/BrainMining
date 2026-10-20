import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/finish_pear_placement.py').read_text(encoding='utf8').replace('unreal.SystemLibrary.quit_editor()','')
exec(compile(code,'finish_pear_placement','exec'))
exec(compile((root/'Scripts/verify_playtest16_ending.py').read_text(encoding='utf8'),'verify_playtest16_ending','exec'))
