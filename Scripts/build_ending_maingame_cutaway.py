"""Bake an ending-only cutaway from the saved MainGame starting excavation region.
Only a copied DynamicMesh is cut; the source map and mining assets are never saved.
"""
import unreal,json,hashlib,bisect
from pathlib import Path

def main():
 root=Path(unreal.Paths.project_dir())
 map_file=root/'Content/MainGame/Maps/Lvl_MainGame.umap'
 before=hashlib.sha256(map_file.read_bytes()).hexdigest()
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_V1'))
 rock=region.generated_rock
 # Subtract the actual authored tunnel from a continuous rock volume. A thin
 # tunnel shell becomes disconnected pipes when sliced; keep the surrounding mass.
 cutter=region.cutters[0]
 tool=unreal.DynamicMesh()
 unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(cutter.mesh,tool,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
 unreal.GeometryScript_MeshTransforms.transform_mesh(tool,cutter.local_transform)
 before_tri=tool.get_triangle_count();assert before_tri>100
 # Unfold only the display copy's lateral meander onto the vertical section.
 # Keep authored X/Z elevation, branches and wall facets; otherwise a straight
 # slice intersects the winding mine only at a few tiny, disconnected holes.
 routes=json.loads((root/'Art/MainGame/RoutesV5/TunnelV1/centerlines.json').read_text())
 main=next(r for r in routes if r['name']=='Main')['points']
 offset=cutter.local_transform.translation
 axis=sorted((p[0]*100+offset.x,-p[1]*100+offset.y) for p in main)
 xs=[p[0] for p in axis]
 def centre_y(x):
  i=max(1,min(len(axis)-1,bisect.bisect_left(xs,x)))
  a,b=axis[i-1],axis[i];t=max(0.,min(1.,(x-a[0])/(b[0]-a[0])))
  return a[1]+(b[1]-a[1])*t
 _,positions,gaps=unreal.GeometryScript_MeshQueries.get_all_vertex_positions(tool,False)
 vertices=unreal.GeometryScript_List.convert_vector_list_to_array(positions)
 unfolded=[unreal.Vector(v.x,v.y-centre_y(v.x),v.z) for v in vertices]
 unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(tool,unreal.GeometryScript_List.convert_array_to_vector_list(unfolded))
 clear_ids=next(getattr(unreal.GeometryScript_Materials,n) for n in dir(unreal.GeometryScript_Materials) if n.replace('_','').lower()=='clearmaterialids')
 clear_ids(tool,1)
 mesh=unreal.DynamicMesh()
 size=region.rock_size
 unreal.GeometryScript_Primitives.append_box(mesh,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),size.x,size.y,size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 clear_ids(mesh,0)
 # Small bevels soften the exterior while retaining the authored mine dimensions.
 for sx in [-1,1]:
  for sz in [-1,1]:
   p=unreal.Vector(sx*(size.x*.5-300),0,sz*(size.z*.5-300))
   n=unreal.Vector(sx,0,sz)
   unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(mesh,unreal.Transform(location=p,rotation=unreal.MathLibrary.make_rot_from_z(n)),unreal.GeometryScriptMeshPlaneCutOptions(fill_holes=True,hole_fill_material_id=0))
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,unreal.Transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions())
 # Cut vertically like a cake, removing the near side and retaining over half.
 normal=unreal.Vector(0,-1,0)
 rotation=unreal.MathLibrary.make_rot_from_z(normal)
 unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(mesh,unreal.Transform(location=unreal.Vector(0,-60,0),rotation=rotation),unreal.GeometryScriptMeshPlaneCutOptions(fill_holes=True,fill_spans=True,hole_fill_material_id=2))
 unreal.GeometryScript_MeshTransforms.transform_mesh(mesh,unreal.Transform(location=unreal.Vector(0,0,360),rotation=unreal.Rotator(yaw=180),scale=unreal.Vector(.1,.1,.1)))
 unreal.GeometryScript_Normals.recompute_normals(mesh,unreal.GeometryScriptCalculateNormalsOptions())
 path='/Game/EndingEmergence/PaperFinale/SM_EE_MainGameCutaway'
 asset=unreal.load_asset(path)
 if asset:
  unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh,asset,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
 else:
  value=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh,path,unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=False))
  asset=value[0] if isinstance(value,tuple) else value
 assert asset
 assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
 assert hashlib.sha256(map_file.read_bytes()).hexdigest()==before
 report={'source_map':'/Game/MainGame/Maps/Lvl_MainGame','source_region':region.get_actor_label(),'source_tunnel':cutter.mesh.get_path_name(),'cut_plane_normal':[0,-1,0],'retained_side':'Y >= -60 cm','display_only_unfolding':'Authored X/Z preserved; main route lateral centre aligned to section','triangles_before':before_tri,'triangles_after':mesh.get_triangle_count(),'asset':asset.get_path_name(),'source_map_unchanged':True,'bounds':str(asset.get_bounding_box())}
 (root/'Saved/Validation/paper_maingame_cutaway.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 unreal.log('PAPER_MAINGAME_CUTAWAY_READY '+str(report))

main()
# Release Python wrappers before Unreal tears down reflected types.
import gc
gc.collect()
