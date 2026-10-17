"""Finalize the P2 television repair and import the museum thought bubble."""
import pathlib
import runpy
import unreal

root = pathlib.Path(unreal.Paths.project_dir())
scripts = root / "Scripts"

# Rebuild the cabinet without the old top-bridge boolean cut, then install the
# authored diggable screen and its collision settings into the P2 map.
runpy.run_path(str(scripts / "prepare_p2_authored_tv.py"), run_name="__main__")
runpy.run_path(str(scripts / "install_p2_authored_tv_repair.py"), run_name="__main__")

# Import the replacement bubble texture after the native Slate code is built.
runpy.run_path(str(scripts / "import_wensi_thought_ui_attachment.py"), run_name="__main__")

assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
unreal.log("P2_TV_AND_WENSI_UI_FINALIZED")
unreal.SystemLibrary.quit_editor()
