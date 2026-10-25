import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BranchClues';L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);report={'passed':False,'regions':[]}
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 for r in sorted([a for a in A.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion)],key=lambda a:a.get_actor_location().x):
  c=r.generated_shell.get_component_by_class(unreal.DynamicMeshComponent);m=c.get_dynamic_mesh();before=m.get_triangle_count()
  unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(m,40000,unreal.GeometryScriptSimplifyMeshOptions())
  assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(m)
  unreal.GeometryScript_Normals.set_per_face_normals(m);c.update_collision(False)
  report['regions'].append({'name':r.get_name(),'before':before,'after':m.get_triangle_count()})
 assert L.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:(out/'boundary-simplify.json').write_text(json.dumps(report,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
