"""Run in Unreal. Bake a repeatable wall section from SM_Vein; never rebuild a level."""
from pathlib import Path
import unreal,json,math,collections
def norm(v):return v/math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z)
out=Path(unreal.Paths.project_dir()).resolve()/'Saved/Validation/EndingArt'
source='/Game/BloodCirculation/Mesh/BloodVessel/SM_Vein'
target='/Game/EndingEmergence/Meshes/SM_EE_BloodCirculationVeinSegment'
assert not unreal.EditorAssetLibrary.does_asset_exist(target), 'Derived asset already exists; inspect before overwriting'
m=unreal.load_asset(source)
base=m.get_material(0)
while isinstance(base,unreal.MaterialInstance):base=base.get_editor_property('parent')
unreal.MaterialEditingLibrary.set_material_usage(base,next(getattr(unreal.MaterialUsage,n) for n in dir(unreal.MaterialUsage) if 'SPLINE' in n.upper()))
assert unreal.EditorAssetLibrary.save_loaded_asset(base)
d=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(m,d,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
_,positions,_=d.get_all_vertex_positions(False);v=unreal.GeometryScript_List.convert_vector_list_to_array(positions)
f=next(a for a in dir(d) if a.replace('_','').lower()=='getmeshpervertexuvs');_,uv,*_=getattr(d,f)(0);uv=unreal.GeometryScript_List.convert_uv_list_to_array(uv)
_,tris,_=d.get_all_triangle_indices(False);tris=unreal.GeometryScript_List.convert_triangle_list_to_array(tris)
# This branch contains nine intact, aligned rings. Retain the actual outer and
# inner surfaces and their UVs, removing only the asset's pre-existing bends.
keep={i for i,p in enumerate(v) if p.y<-5000 and 16.32415<=uv[i].x<=17.0827}
groups=collections.defaultdict(list)
for i in keep:groups[round(uv[i].x,4)].append(i)
keys=sorted(groups);assert len(keys)==9 and all(len(groups[k])==32 for k in keys),str([(k,len(groups[k])) for k in keys])
centers={k:sum((v[i] for i in groups[k]),unreal.Vector())/32 for k in keys}
new={};crosses=collections.defaultdict(list)
for j,k in enumerate(keys):
 c=centers[k];t=norm(centers[keys[min(j+1,len(keys)-1)]]-centers[keys[max(0,j-1)]]);side=norm(unreal.Vector(-t.y,t.x,0))
 for i in groups[k]:
  q=v[i]-c;cross=unreal.Vector(0,q.dot(side),q.z)
  new[i]=[j/(len(keys)-1)*1000,cross.y,cross.z]
  # UV V identifies corresponding points on both sides of the tube wall.
  if j in (0,len(keys)-1):crosses[round(uv[i].y,3)].append(cross)
for k in (keys[0],keys[-1]):
 for i in groups[k]:
  pair=crosses[round(uv[i].y,3)];assert len(pair)==2
  avg=(pair[0]+pair[1])*.5;new[i][1:]=[avg.y,avg.z]
remove=[j for j,t in enumerate(tris) if not all(i in keep for i in [t.x,t.y,t.z])]
ids=unreal.GeometryScript_List.convert_array_to_index_list(remove)
d.delete_triangles_from_mesh(ids,True)
for i,p in new.items():d.set_vertex_position(i,unreal.Vector(*p),True)
d.compact_mesh();d.recompute_normals(unreal.GeometryScriptCalculateNormalsOptions())
options=unreal.GeometryScriptCreateNewStaticMeshAssetOptions();options.enable_collision=False;options.enable_recompute_normals=True;options.enable_recompute_tangents=True
asset,result=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(d,target,options)
assert asset,str(result);asset.set_material(0,m.get_material(0));assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
(out/'vein_bake.json').write_text(json.dumps(dict(source=source,target=target,source_triangles=len(tris),kept_triangles=asset.get_num_triangles(0),rings=len(keys),length=1000,wall_uvs_preserved=True),indent=2))
unreal.SystemLibrary.quit_editor()
