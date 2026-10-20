from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_vessel_lighting_feedback.py').read_text(encoding='utf8')
code=code.replace("'Saved/Validation/TraversalFeedback/Blood'","'Saved/Validation/Feedback11/Blood'")
code=code.replace('c.intensity==18000','c.intensity==900')
code=code.replace("unreal.EditorPythonScripting.set_keep_python_script_alive(False)","unreal.EditorPythonScripting.set_keep_python_script_alive(False);unreal.SystemLibrary.quit_editor()")
exec(compile(code,str(root/'Scripts/verify_vessel_lighting_feedback.py'),'exec'))
