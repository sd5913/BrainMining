"""Ensure all authored assets are safely saved before reloading native code."""
import unreal,json
from pathlib import Path
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not dirty,[p.get_path_name() for p in dirty]
root=Path(unreal.Paths.project_dir()).resolve()
(root/'Saved/Validation/OfficeC4/prebuild.json').write_text(json.dumps({'passed':True,'dirty_packages':[]}),encoding='utf-8')
unreal.log('OFFICE_C4_ASSETS_SAVED_FOR_REBUILD')
