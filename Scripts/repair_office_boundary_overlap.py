"""Trim only the two passage shells that protruded through the authored office walls."""
import unreal,json,shutil
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Collection24';out.mkdir(parents=True,exist_ok=True)
src=root/'Content/MainGame/Maps/Lvl_MainGame.umap';backup=out/'Before/Lvl_MainGame.umap'
if not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,backup)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert L.set_current_level_by_name('Lvl_MainGame')
# Five cm behind each original room-facing wall, below the carpet and above the tiles.
# Do not alter the room's imported materials, collision, furnishings or any other shell.
tool=unreal.DynamicMesh();identity=unreal.Transform()
unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=unreal.Vector(-15750,0,6590)),1990,1480,400,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
report=[]
for label in ['LINK_Mine01_Office_BedrockShell','LINK_Office_Mine02_BedrockShell']:
 a=next(x for x in A.get_all_level_actors() if x.get_actor_label()==label)
 c=a.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh();before=mesh.get_triangle_count()
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,a.get_actor_transform(),tool,identity,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
 assert mesh.get_triangle_count()>0 and unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh),label
 unreal.GeometryScript_Normals.set_per_face_normals(mesh);c.set_complex_as_simple_collision_enabled(True,True);c.update_collision(False)
 assert c.get_material(0).get_name()=='M_BedrockDark'
 report.append(dict(actor=label,before=before,after=mesh.get_triangle_count(),closed=True,passage_stone_preserved=True))
assert L.save_current_level()
(out/'office-wall-repair.json').write_text(json.dumps(report,indent=2))
unreal.SystemLibrary.quit_editor()
