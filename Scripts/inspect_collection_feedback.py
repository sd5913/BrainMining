import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/CollectionFeedback';out.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'passed':False}
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 world=ed.get_editor_world();hall=next(a for a in api.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
 sub='/Game/MainGame/Maps/Lvl_MainGame_SubtitleTriggers'
 stream=unreal.GameplayStatics.get_streaming_level(world,sub)
 if not stream:stream=unreal.EditorLevelUtils.add_level_to_world(world,sub,unreal.LevelStreamingAlwaysLoaded)
 assert stream and stream.get_loaded_level()
 triggers=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SubtitleTrigger)
 remote=[a for a in triggers if a.get_level()==stream.get_loaded_level()]
 for a in [a for a in triggers if a.get_level()==hall.get_level()]:
  match=next((b for b in remote if b.segment_id==a.segment_id and b.get_actor_location().distance(a.get_actor_location())<.01),None)
  assert match,'Unmatched local subtitle trigger '+a.get_actor_label()
  api.destroy_actor(a)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/MainGame/Maps/Lvl_MainGame')
 def row(a):
  c,e=a.get_actor_bounds(False);r=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),p=list(a.get_actor_location().to_tuple()),rot=list(a.get_actor_rotation().to_tuple()),extent=list(e.to_tuple()),tags=[str(t) for t in a.tags])
  if isinstance(a,unreal.DiggableRock):r['material']=a.rock_material.get_path_name() if a.rock_material else None
  if isinstance(a,unreal.StaticMeshActor):r['mesh']=a.static_mesh_component.static_mesh.get_path_name() if a.static_mesh_component.static_mesh else None;r['materials']=[m.get_path_name() if m else None for m in a.static_mesh_component.get_materials()]
  return r
 report['actors']=[row(a) for a in api.get_all_level_actors() if a.get_level()==hall.get_level() or 'Entrance' in a.get_class().get_name()]
 report['areas']=[dict(tag=str(a.tag),origin=list(a.origin.to_tuple()),route=[list(v.to_tuple()) for v in a.map_route]) for a in hall.areas]
 report['subtitle_count']=len(remote);report['passed']=True
except Exception:report['error']=traceback.format_exc()
(out/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
