"""Run the actual P2 drill/capsule sweep through all 70 m, then collect a landed ribbon."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/GameplayFollowup/TV';out.mkdir(parents=True,exist_ok=True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
code=(root/'Scripts/verify_card_p2_flow.py').read_text(encoding='utf-8')
code=code.replace("/'Saved/Validation/CardP2Flow'","/'Saved/Validation/GameplayFollowup/TV'")
code=code.replace("if phase==12:\n   effects", "if phase==12:\n   if not e.get_editor_property('stage_ribbon').released:s['next']=now+.2;return\n   effects")
code=code.replace("unreal.EditorPythonScripting.set_keep_python_script_alive(False);return", "levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor();return")
exec(compile(code,str(root/'Scripts/verify_card_p2_flow.py'),'exec'))
