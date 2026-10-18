import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/GameplayFollowup';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={}
def row(a):
 c,e=a.get_actor_bounds(False)
 r=dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),level=a.get_level().get_path_name(),p=list(a.get_actor_location().to_tuple()),r=list(a.get_actor_rotation().to_tuple()),s=list(a.get_actor_scale3d().to_tuple()),center=list(c.to_tuple()),extent=list(e.to_tuple()),meshes=[])
 for m in a.get_components_by_class(unreal.StaticMeshComponent):
  r['meshes'].append(dict(name=m.get_name(),mesh=m.static_mesh.get_path_name() if m.static_mesh else None,materials=[x.get_path_name() if x else None for x in m.get_materials()],collision=str(m.get_collision_enabled()),p=list(m.get_world_location().to_tuple())))
 if isinstance(a,unreal.PostProcessVolume):r.update(settings=str(a.settings),enabled=a.enabled,unbound=a.unbound)
 return r
try:
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=api.get_all_level_actors()
 report['actors']=[row(a) for a in aa if any(k in (a.get_actor_label()+a.get_class().get_name()).lower() for k in ['管','faucet','ruler','尺','television','postprocess','treasure','collectionfield','echo','roomescape'])]
 report['passed']=True
except Exception:report['error']=traceback.format_exc()
(out/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
