import unreal,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/ScenePolish';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
try:
 for path in (['/Game/MainGame/Maps/Lvl_ScenePolishBaseline'] if '-Baseline' in unreal.SystemLibrary.get_command_line() else ['/Game/MainGame/Maps/Lvl_MainGame','/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet']):
  assert levels.load_level(path)
  rows=[]
  for a in actors.get_all_level_actors():
   p,e=a.get_actor_bounds(False)
   row=dict(name=a.get_name(),label=a.get_actor_label(),folder=str(a.get_folder_path()),hidden=a.get_editor_property('hidden'),cls=a.get_class().get_name(),level=a.get_level().get_path_name(),pos=list(a.get_actor_location().to_tuple()),rot=str(a.get_actor_rotation()),scale=list(a.get_actor_scale3d().to_tuple()),bounds=[list(p.to_tuple()),list(e.to_tuple())],tags=[str(t) for t in a.tags],components=[])
   for c in a.get_components_by_class(unreal.StaticMeshComponent):
    row['components'].append(dict(name=c.get_name(),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()],pos=list(c.get_world_location().to_tuple()),collision=str(c.get_collision_enabled())))
   if isinstance(a,unreal.NiagaraActor):
    c=a.get_component_by_class(unreal.NiagaraComponent);row['vfx']=c.get_asset().get_path_name() if c.get_asset() else None
   if isinstance(a,unreal.MiningCollectionField):
    row['placements']=[dict(id=p.item_id,pos=list(p.transform.translation.to_tuple()),presentation=p.presentation_actor.get_name() if p.presentation_actor else None,clue=list(p.clue_transform.translation.to_tuple())) for p in a.authored_placements]
   rows.append(row)
  report[path]=rows
 (out/('baseline.json' if '-Baseline' in unreal.SystemLibrary.get_command_line() else 'inspection.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
except Exception:(out/'inspection_error.txt').write_text(traceback.format_exc(),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
