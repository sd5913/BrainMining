"""Keep a clear gap between the thank-you line and developer names in UMG."""
import unreal

assert unreal.ProjectTypographyEditorLibrary.install_credits_fonts()
assert unreal.MiningGUIEditorLibrary.update_ending_credits_feedback()
unreal.SystemLibrary.quit_editor()
