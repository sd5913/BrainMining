import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Saved/Validation/P2Repair'
src=unreal.load_asset('/Game/Countryside_Interior/Meshes/SM_TVSet')
groups=json.loads((out/'tv_groups.json').read_text()); glass_ids=set(groups[1]['ids'])
def source():
 d=unreal.DynamicMesh()
 unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(src,d,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
 return d
def delete(d,ids):
 unreal.GeometryScript_MeshEdits.delete_triangles_from_mesh(d,unreal.GeometryScript_List.convert_array_to_index_list(list(ids)))
def save(d,name):
 path='/Game/Passages/Echo/ArtP2/Television/'+name
 asset=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name(),path)
 unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(d,asset,unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False,enable_recompute_tangents=True),unreal.GeometryScriptMeshWriteLOD())
 unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(asset)
 setup=asset.get_editor_property('body_setup');setup.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
 assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
 return asset
glass=source(); delete(glass,set(range(glass.get_triangle_count()))-glass_ids)
unreal.GeometryScript_MeshRepair.weld_mesh_edges(glass,unreal.GeometryScriptWeldEdgesOptions(tolerance=.001,only_unique_pairs=True))
closed=unreal.GeometryScript_MeshQueries.get_is_closed_mesh(glass)
if not closed:unreal.GeometryScript_MeshRepair.fill_all_mesh_holes(glass,unreal.GeometryScriptFillHolesOptions())
assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(glass),'Authored glass must be a closed boolean solid'
save(glass,'SM_P2_TV_Glass')
case=source();delete(case,glass_ids)
# Keep the complete cabinet shell.  The previous cleanup subtracted a large
# box through the top bridge, which left the two upper halves visibly broken.
# The authored DiggableRock screen is already a separate closed solid and the
# cabinet actor is set to NoCollision, so no extra opening cut is needed here.
save(case,'SM_P2_TV_Cabinet')
(out/'tv_prepared.json').write_text(json.dumps({'closed_glass':True,'glass_triangles':glass.get_triangle_count(),'cabinet_triangles':case.get_triangle_count()}))
