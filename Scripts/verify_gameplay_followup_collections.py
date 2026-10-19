"""Re-use the actual excavation/E input suite with this round's saved placements."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
wrapper=(root/'Scripts/verify_collection_feedback.py').read_text(encoding='utf-8')
exec(wrapper.split('exec(compile(code,')[0])
code=code.replace("feedback=json.loads((OUT/'authoring.json').read_text())","feedback=json.loads((ROOT/'Saved/Validation/CollectionFeedback/authoring.json').read_text())")
code=code.replace("auth['placements']=feedback['placements']","auth['placements']=json.loads((ROOT/'Saved/Validation/GameplayFollowup/authoring.json').read_text())['placements']")
code=code.replace("OUT=ROOT/'Saved/Validation/CollectionFeedback'","OUT=ROOT/'Saved/Validation/GameplayFollowup'")
code=code.replace("s.update(escape=escape,phase=16,next=now+1)","s.update(escape=escape,phase=16,next=now+8)")
code=code.replace("ribbon=s['escape'].stage_ribbon\n", "ribbon=s['escape'].stage_ribbon\n            check('Ribbon is available only after its cannon flight',ribbon.released and not ribbon.get_editor_property('hidden'))\n")
exec(compile(code,str(root/'Scripts/verify_main_game_collections.py'),'exec'))
