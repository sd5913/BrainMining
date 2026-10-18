"""Recheck the TV signal color in MainGame without replaying unrelated checks."""
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/capture_gameplay_followup.py').read_text(encoding='utf-8')
code=code.replace("OUT/'visual.json'", "OUT/'screen.json'")
code=code.replace("   if not s.get('starter'):", """   area(3)
   e=actors(unreal.MiningRoomEscape)[0];stage=actors(unreal.CardRoomStage)[0]
   e.call_method('OnRewardClaimed',args=(stage.get_editor_property('room_id'),));e.retry_escape()
   entry=e.get_editor_property('computer_entry');pose(entry+unreal.Vector(-1100,-400,-60),entry)
   s.update(escape=e,phase=13,next=now+1);return
   if not s.get('starter'):""")
exec(compile(code,str(root/'Scripts/capture_gameplay_followup.py'),'exec'))
