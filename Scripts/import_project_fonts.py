"""Install embedded Source Han Sans fonts and update project UI font references.

Run in the editor with PIE stopped after building BrainMiningEditor.
Reads SourceAssets/Fonts; saves /Game/Fonts and project widget blueprints.
No map packages are saved. Details are written to
Saved/Validation/ProjectFonts/installed.json.
"""
import unreal

assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
assert unreal.ProjectTypographyEditorLibrary.install_project_fonts(), 'Font installation failed; inspect the editor log'
from pathlib import Path
metrics_script = Path(unreal.Paths.project_dir()) / 'Scripts/fix_project_font_metrics.py'
exec(compile(metrics_script.read_text(encoding='utf-8-sig'), str(metrics_script), 'exec'))
unreal.log('SOURCE_HAN_SANS_INSTALLED: titles/subtitles=Bold, body/controls=Medium')
