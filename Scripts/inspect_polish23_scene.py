"""Inspect authored targets before changing their interaction bindings."""
import unreal,json
from pathlib import Path
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
result={}
for map in ['/Game/StartRoom/Maps/Lvl_StartRoom','/Game/MainGame/Maps/Lvl_MainGame']:
 assert levels.load_level(map)
 rows=[]
 for actor in a.get_all_level_actors():
  tags=[str(t) for t in actor.tags]
  label=actor.get_actor_label()
  if any(t.startswith('StartRoomView_') for t in tags) or 'toilet' in label.lower() or '马桶' in label or 'BM_BombOnly' in tags:
   meshes=[str(c.static_mesh.get_path_name()) for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
   rows.append(dict(name=actor.get_name(),label=label,tags=tags,location=str(actor.get_actor_location()),bounds=str(actor.get_actor_bounds(False)),meshes=meshes))
 result[map]=rows
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Polish23';out.mkdir(parents=True,exist_ok=True)
(out/'scene.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
