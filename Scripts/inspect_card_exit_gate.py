import unreal,json
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
hall=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
rows=[]
for i,a in enumerate(hall.areas):
 g=a.progress_gate
 rows.append(dict(index=i,tag=str(a.tag),exit=str(a.exit),gate=None if not g else dict(name=g.get_name(),label=g.get_actor_label(),path=g.get_path_name(),position=g.get_actor_location().to_tuple(),bounds=str(g.get_actor_bounds(False)),tags=[str(t) for t in g.tags])))
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardExitGate';out.mkdir(parents=True,exist_ok=True)
(out/'inspection.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
