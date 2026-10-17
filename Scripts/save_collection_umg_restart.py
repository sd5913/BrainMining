import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Automation/CollectionUMG';out.mkdir(parents=True,exist_ok=True)
e=unreal.EditorLoadingAndSavingUtils
packages=list(e.get_dirty_content_packages())+list(e.get_dirty_map_packages())
(out/'before_restart.json').write_text(json.dumps({'dirty':[p.get_name() for p in packages],'map':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()},indent=2),encoding='utf-8')
assert e.save_packages(packages,only_dirty=True) if packages else True
unreal.SystemLibrary.quit_editor()
