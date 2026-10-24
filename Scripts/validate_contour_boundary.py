import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BranchClues';L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);I=unreal.Transform();report={'passed':False,'regions':[]};fixtures=[]
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(A.get_all_level_actors())
 for index,r in enumerate(sorted([a for a in aa if isinstance(a,unreal.MiningExternalRegion)],key=lambda a:a.get_actor_location().x),1):
  core=unreal.DynamicMesh()
  for cutter in r.cutters:
   if not cutter.mesh or not cutter.enabled:continue
   t=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(cutter.mesh,t,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0));unreal.GeometryScript_MeshEdits.append_mesh(core,t,cutter.local_transform)
  _,bvh=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(core)
  wall=r.generated_shell.get_component_by_class(unreal.DynamicMeshComponent).get_dynamic_mesh();assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(wall)
  _,vl,_=unreal.GeometryScript_MeshQueries.get_all_vertex_positions(wall,False);vs=list(unreal.GeometryScript_List.convert_vector_list_to_array(vl));distances=[];candidates=[]
  for v in vs[::max(1,len(vs)//700)]:
   if abs(v.x)>r.rock_size.x*.5-600:continue
   _,hit,_=unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(core,bvh,v,unreal.GeometryScriptSpatialQueryOptions());assert hit.valid
   d=v.distance(hit.position);distances.append(d)
   if 1930<d<2070 and (abs(v.y)>r.rock_size.y*.5+100 or abs(v.z)>r.rock_size.z*.5+100):
    p=v+r.get_actor_location();n=(v-hit.position)/d
    if index!=3 or p.z<17100:candidates.append((abs(v.x),p,n))
  assert distances and min(distances)>1800 and max(distances)<2450,(index,min(distances),max(distances))
  buffer=next(a for a in aa if a.get_actor_label()==f'MINE_{index:02}_Contour20m_DiggableBuffer')
  assert candidates
  _,point,normal=min(candidates,key=lambda q:q[0]);fixtures.append({'region':index,'point':list(point.to_tuple()),'normal':list(normal.to_tuple()),'shell':r.generated_shell.get_name(),'buffer':buffer.get_name()})
  report['regions'].append({'region':index,'samples':len(distances),'min_cm':min(distances),'max_cm':max(distances),'closed':True,'triangles':wall.get_triangle_count()})
 (root/'Config/ContourBoundaryChecks.json').write_text(json.dumps(fixtures,indent=2),encoding='utf8');report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:(out/'boundary-validation.json').write_text(json.dumps(report,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
