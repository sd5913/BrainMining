"""Remove a short central section from the original brow, preserving all outer vertices and UVs."""
import unreal,json,math,hashlib,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/Playtest18';out.mkdir(parents=True,exist_ok=True)
src='/Game/Rooms/CardDuel/Robot/SM_Robot_BrowL'
dst=src+'_Singed'
EA=unreal.EditorAssetLibrary;AU=unreal.GeometryScript_AssetUtils
r={'passed':False}

def points(mesh):
    return list(unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh,False)[1]))

def load_mesh(asset):
    mesh=unreal.DynamicMesh()
    AU.copy_mesh_from_static_mesh_v2(asset,mesh,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0))
    return mesh

try:
    filename=root/'Content/Rooms/CardDuel/Robot/SM_Robot_BrowL.uasset'
    before=hashlib.sha256(filename.read_bytes()).hexdigest()
    original=unreal.load_asset(src);assert original
    mesh=load_mesh(original);v=points(mesh)
    center=sum(v,unreal.Vector())/len(v)
    covariance=[[sum((p.to_tuple()[i]-center.to_tuple()[i])*(p.to_tuple()[j]-center.to_tuple()[j]) for p in v) for j in range(3)] for i in range(3)]
    axis=[0.,1.,0.]
    for _ in range(32):
        axis=[sum(covariance[i][j]*axis[j] for j in range(3)) for i in range(3)]
        length=math.sqrt(sum(x*x for x in axis));axis=[x/length for x in axis]
    direction=unreal.Vector(*axis)
    projected=[(p-center).dot(direction) for p in v]
    gap=(max(projected)-min(projected))*.08
    # Each cut retains its original half without moving, shrinking or rotating it.
    halves=[]
    for sign in [-1,1]:
        half=load_mesh(original)
        plane=center+direction*(sign*gap*.5)
        normal=direction*(-sign)
        unreal.GeometryScript_MeshBooleans.apply_mesh_plane_cut(half,unreal.Transform(location=plane,rotation=unreal.MathLibrary.make_rot_from_z(normal)),unreal.GeometryScriptMeshPlaneCutOptions(fill_holes=True,fill_spans=True,hole_fill_material_id=0))
        halves.append(half)
    result=unreal.DynamicMesh()
    for half in halves:unreal.GeometryScript_MeshEdits.append_mesh(result,half,unreal.Transform())
    kept=points(result)
    assert result.get_triangle_count()>mesh.get_triangle_count()
    assert all(any(p.distance(q)<.001 for q in kept) for p in v),'An original end vertex was moved or lost'
    assert all(abs((p-center).dot(direction))>=gap*.5-.001 for p in kept),'Cut left vertices inside the central gap'
    triangles=unreal.GeometryScript_List.convert_triangle_list_to_array(unreal.GeometryScript_MeshQueries.get_all_triangle_indices(result,False)[1])
    for tri in triangles:
        d=[(kept[i]-center).dot(direction) for i in [tri.x,tri.y,tri.z]]
        assert max(d)<=-gap*.5+.001 or min(d)>=gap*.5-.001,'A triangle bridges the missing section'
    asset=unreal.load_asset(dst) if EA.does_asset_exist(dst) else EA.duplicate_asset(src,dst)
    options=unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False,enable_recompute_tangents=False)
    AU.copy_mesh_to_static_mesh(result,asset,options,unreal.GeometryScriptMeshWriteLOD())
    for slot,material in enumerate(original.static_materials):asset.set_material(slot,material.material_interface)
    assert EA.save_loaded_asset(asset,False)
    assert hashlib.sha256(filename.read_bytes()).hexdigest()==before,'Source eyebrow changed'
    r.update(passed=True,asset=dst,gap_fraction=.08,gap_cm=gap,axis=axis,original_vertices=len(v),result_vertices=len(kept),original_asset_unchanged=True)
except Exception:
    r['error']=traceback.format_exc();unreal.log_error(r['error'])
(out/'singed-brow.json').write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
