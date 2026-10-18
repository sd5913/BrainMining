"""Shared helpers for editing saved terrain and authoring collision-matched platforms."""
import unreal,sys,math,hashlib,shutil,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
from gameplay_feedback_geometry import mushroom
lib=unreal.MiningDesignerLibrary;assets=unreal.EditorAssetLibrary;I=unreal.Transform()
def V(p):return unreal.Vector(*p)
def setp(o,k,v):o.set_editor_property(k,v,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
def points(a,source):
    pts=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(source,False)[1])
    return [unreal.MathLibrary.transform_location(a.get_actor_transform(),p) for p in pts]
def bounds(a):
    pts=points(a,lib.get_campaign_rock_source(a));return V([min(p.to_tuple()[i] for p in pts) for i in range(3)]),V([max(p.to_tuple()[i] for p in pts) for i in range(3)])
def deform(a,warp):
    m=lib.get_campaign_rock_source(a)
    for i,p in enumerate(points(a,m)):
        q=unreal.MathLibrary.inverse_transform_location(a.get_actor_transform(),warp(p));_,ok=unreal.GeometryScript_MeshEdits.set_vertex_position(m,i,q,True);assert ok
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(m)
    unreal.GeometryScript_Normals.set_per_face_normals(m);assert lib.set_campaign_rock_source(a,m)
def material(a,m):
    setp(a,'rock_material',m)
    for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,m)
def mesh_from_buffers(vertices,faces):
    m=unreal.DynamicMesh();unreal.GeometryScript_MeshEdits.append_buffers_to_mesh(m,unreal.GeometryScriptSimpleMeshBuffers(vertices=[V(p) for p in vertices],uv0=[unreal.Vector2D(p[0]/100,p[1]/100) for p in vertices],triangles=[unreal.IntVector(*t) for t in faces]))
    unreal.GeometryScript_Normals.set_per_face_normals(m)
    assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(m)
    return m
def platform(a,base,top,radius,seed,mat,name):
    m=mesh_from_buffers(*mushroom(top.z-base.z,radius,seed))
    path='/Game/MainGame/Models/Feedback/'+name
    model=unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if model:unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(m,model,unreal.GeometryScriptCopyMeshToAssetOptions(enable_recompute_normals=False),unreal.GeometryScriptMeshWriteLOD())
    else:
        value=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(m,path,unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=True));model=value[0] if isinstance(value,tuple) else value
    assert model
    body=model.get_editor_property('body_setup');body.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    body.set_editor_property('double_sided_geometry',True);assert assets.save_loaded_asset(model)
    a.static_mesh_component.set_static_mesh(model);a.static_mesh_component.set_material(0,mat);a.static_mesh_component.set_collision_profile_name('BlockAll')
    a.set_actor_scale3d(V([1,1,1]));a.set_actor_rotation(unreal.Rotator(),True);a.set_actor_location(base,False,True)
    setp(a,'tags',list(a.tags)+['BM_Feedback_RockPlatform'])
    return dict(actor=a.get_name(),label=a.get_actor_label(),base=list(base.to_tuple()),top=list(top.to_tuple()),radius=radius,model=path)
def backup_asset(asset):
    folder=Path((ROOT/'Saved/Validation/GameplayFeedback/backup.txt').read_text());package=asset.get_path_name().split('.')[0]
    assert package.startswith('/Game/')
    f=ROOT/'Content'/(package[6:]+'.uasset');dest=folder/'Content'/(package[6:]+'.uasset')
    if f.exists() and not dest.exists():dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
