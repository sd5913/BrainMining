"""Verify the already saved deletion without repeating the authoring operation."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
code=(root/'Scripts/remove_card_exit_gate.py').read_text(encoding='utf8')
start=code.index(" check(L.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Load latest MainGame map')")
end=code.index(' H.start_floating_preview()',start)
code=code[:start]+""" check(L.load_level('/Game/MainGame/Maps/Lvl_MainGame'),'Reload saved MainGame')
 hall=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
 index=next(i for i,a in enumerate(hall.areas) if str(a.tag)=='BM_Campaign_MainGameJourney')
 check(hall.areas[index].progress_gate is None,'Completion gate reference is cleared')
 check(not any(a.get_actor_label()=='LINK_Journey_CompletionGate' for a in A.get_all_level_actors()),'Completion wall actor is deleted')
 position=unreal.Vector(33045,11750,20080);backup='Already backed up by removal script'
"""+code[end:]
code=code.replace("check(not hit or not hit.to_tuple()[0],'Former wall is clear at '+str((y,z)))","""h=hit.to_tuple() if hit else None
   r.setdefault('traces',[]).append({'offset':[y,z],'hit':str(h)})
   check(not h or not h[0] or h[9].get_actor_label()!='LINK_Journey_CompletionGate','No remaining completion-wall collision at '+str((y,z)))""")
code=code.replace("OUT/'result.json'","OUT/'runtime.json'")
exec(compile(code,'verify_card_exit_gate','exec'))
