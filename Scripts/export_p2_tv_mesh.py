import unreal,json
from pathlib import Path
m=unreal.load_asset('/Game/Countryside_Interior/Meshes/SM_TVSet')
d=unreal.DynamicMesh()
unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(m,d,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
v=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(d,False)[1])
t=unreal.GeometryScript_List.convert_triangle_list_to_array(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(d,False)[1])
Path(unreal.Paths.project_dir()+'Saved/Validation/P2Repair/tv_geometry.json').write_text(json.dumps({'v':[[p.x,p.y,p.z] for p in v],'t':[[p.x,p.y,p.z] for p in t]}))
