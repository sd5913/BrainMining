"""Check vessel transitions and earlier credits in the production ending map."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/verify_playtest16_ending.py').read_text(encoding='utf8')
code=code.replace("code=code.replace('Saved/Validation/TraversalFeedback/Blood','Saved/Validation/Playtest16/Ending')", "code=code.replace('Saved/Validation/TraversalFeedback/Blood','Saved/Validation/Playtest17/Ending')")
code=code.replace("next=now+10", "next=now+5.5")
exec(compile(code,'playtest17_ending','exec'))
