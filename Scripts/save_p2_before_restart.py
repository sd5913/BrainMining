import unreal,json
from pathlib import Path
E=unreal.EditorLoadingAndSavingUtils
packages=list(E.get_dirty_content_packages())+list(E.get_dirty_map_packages())
Path(unreal.Paths.project_dir(),'Saved/Automation/CountrysideP2/dirty_before_restart.json').write_text(json.dumps([p.get_name() for p in packages],indent=2),encoding='utf-8')
assert E.save_packages(packages,only_dirty=True) if packages else True
unreal.log('P2_WORK_SAVED_FOR_RESTART')
unreal.SystemLibrary.quit_editor()
