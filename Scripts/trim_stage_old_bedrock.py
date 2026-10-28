"""Remove the redundant grey cave collar inside the new stage enclosure.

Keep the downstream collar and 150 cm of overlap with the permanent runtime
backing (which ends at X=33230). The walking aperture is not filled or narrowed.
"""
import json,shutil,unreal
from pathlib import Path

root=Path(unreal.Paths.project_dir())
out=root/'Saved/Validation/StageBoundary';out.mkdir(parents=True,exist_ok=True)
source=root/'Content/MainGame/Maps/Lvl_MainGame.umap'
backup=out/'BeforeOldCollarTrim/Lvl_MainGame.umap'
if not backup.exists():
 backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
assert levels.set_current_level_by_name('Lvl_MainGame')
a=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='LINK_Journey_Mine03_BedrockShell')
assert a.get_path_name().startswith('/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame:PersistentLevel.')
c=a.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh()
assert c.get_material(0).get_name()=='M_BedrockDark'
center,extent=a.get_actor_bounds(False);lo=center-extent;hi=center+extent
before=mesh.get_triangle_count()
if lo.x<33079:
 assert 32600<lo.x<32700 and hi.x>33900,(lo,hi)
 cut_lo=lo-unreal.Vector(100,100,100)
 cut_hi=unreal.Vector(33080,hi.y+100,hi.z+100)
 size=cut_hi-cut_lo
 tool=unreal.DynamicMesh()
 unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=(cut_lo+cut_hi)*.5),size.x,size.y,size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,a.get_actor_transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
 assert mesh.get_triangle_count()>0 and unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh)
 unreal.GeometryScript_Normals.set_per_face_normals(mesh)
 c.set_complex_as_simple_collision_enabled(True,True);c.update_collision(False)
 a.set_editor_property('tags',list(a.tags)+['BM_StageOldCollarTrim'])
 assert levels.save_current_level()
bounds=unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
world_min=unreal.MathLibrary.transform_location(a.get_actor_transform(),bounds.min)
assert abs(world_min.x-33080)<1,str(world_min)
(out/'old-collar-trim.json').write_text(json.dumps({'passed':True,'actor':a.get_actor_label(),'before_triangles':before,'after_triangles':mesh.get_triangle_count(),'remaining_min_x':world_min.x,'backing_overlap_cm':150,'backup':str(backup)},indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
