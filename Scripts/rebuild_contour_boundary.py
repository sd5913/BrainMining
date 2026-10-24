"""Contour bedrock 20 m outside the current tunnel cutters; preserve authored inner terrain and room connections."""
import unreal,json,traceback,shutil,time
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/BranchClues';L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);I=unreal.Transform();report={'passed':False,'regions':[]}
G=unreal.GeometryScript_MeshEdits;B=unreal.GeometryScript_MeshBooleans;Q=unreal.GeometryScript_MeshQueries

def clone(src):
 m=unreal.DynamicMesh();G.append_mesh(m,src,I);return m

def box(center,size):
 m=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_box(m,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=center),size.x,size.y,size.z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER);return m

def boolean(a,b,op):
 original=clone(a)
 for nudge in [0.,.137,.413,1.17]:
  a.reset();G.append_mesh(a,original,I)
  transform=unreal.Transform(location=unreal.Vector(nudge,nudge*.71,nudge*.37))
  B.apply_mesh_boolean(a,I,b,transform,op,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
  if a.get_triangle_count()==0 or Q.get_is_closed_mesh(a):return a
  unreal.GeometryScript_MeshRepair.weld_mesh_edges(a,unreal.GeometryScriptWeldEdgesOptions(tolerance=.1))
  if Q.get_is_closed_mesh(a):return a
 # Reconstruct a watertight solid from the winding field when boolean seam topology is degenerate.
 repair_grid=unreal.GeometryScript3DGridParameters(size_method=unreal.GeometryScriptGridSizingMethod.GRID_CELL_SIZE,grid_cell_size=50)
 unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_solidify(a,unreal.GeometryScriptSolidifyOptions(grid_parameters=repair_grid,surface_search_steps=6))
 assert a.get_triangle_count()>0 and Q.get_is_closed_mesh(a),'Voxel repair did not close the shell'
 report.setdefault('voxel_seam_repairs',0);report['voxel_seam_repairs']+=1
 return a

def write_progress(): (out/'contour-boundary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 aa=list(A.get_all_level_actors());regions=sorted([a for a in aa if isinstance(a,unreal.MiningExternalRegion)],key=lambda a:a.get_actor_location().x)
 assert len(regions)==3
 assert not any(a.actor_has_tag('BM_ContourBedrock20m') for a in aa),'Already rebuilt; restore the saved pre-boundary backup before regenerating'
 backup=root/'Saved/Backups'/('BeforeContourBoundary-'+str(int(time.time()))+'.umap');shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup)
 for index,region in enumerate(regions,1):
  origin=region.get_actor_location();size=region.rock_size;record={'region':index,'status':'building','offset_cm':2000,'grid_cm':75};report['regions'].append(record);write_progress()
  # Use the exact currently assigned cutter meshes, never the retired cubic envelopes or a centerline approximation.
  core=unreal.DynamicMesh()
  for cutter in region.cutters:
   if not cutter.mesh or not cutter.enabled:continue
   tool=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(cutter.mesh,tool,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0));G.append_mesh(core,tool,cutter.local_transform)
  grid=unreal.GeometryScript3DGridParameters(size_method=unreal.GeometryScriptGridSizingMethod.GRID_CELL_SIZE,grid_cell_size=75)
  inner=clone(core);unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_morphology(inner,unreal.GeometryScriptMorphologyOptions(sdf_grid_parameters=grid,operation=unreal.GeometryScriptMorphologicalOpType.DILATE,distance=2000));assert inner.get_triangle_count()>100 and Q.get_is_closed_mesh(inner)
  _,bvh=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(core)
  _,vlist,_=Q.get_all_vertex_positions(inner,False);vertices=list(unreal.GeometryScript_List.convert_vector_list_to_array(vlist));distances=[];samples=[]
  for v in vertices[::max(1,len(vertices)//100)]:
   _,nearest,_=unreal.GeometryScript_MeshSpatial.find_nearest_point_on_mesh(core,bvh,v,unreal.GeometryScriptSpatialQueryOptions())
   assert nearest.valid;distance=v.distance(nearest.position);distances.append(distance)
   if abs(v.x)<size.x*.5-600 and (abs(v.y)>size.y*.5+100 or abs(v.z)>size.z*.5+100):
    n=(v-nearest.position)/distance;samples.append({'point':list((v+origin).to_tuple()),'normal':list(n.to_tuple())})
  record.update(offset_measured_min=min(distances),offset_measured_max=max(distances),samples=samples);assert min(distances)>1850 and max(distances)<2150
  outer=clone(core);unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_morphology(outer,unreal.GeometryScriptMorphologyOptions(sdf_grid_parameters=grid,operation=unreal.GeometryScriptMorphologicalOpType.DILATE,distance=2200));assert Q.get_is_closed_mesh(outer)
  # Leave existing end interfaces intact: expanding into rooms would cover authored interiors.
  slab=box(unreal.Vector(),unreal.Vector(size.x,100000,100000))
  soil=boolean(clone(inner),slab,unreal.GeometryScriptBooleanOperation.INTERSECTION)
  soil=boolean(soil,box(unreal.Vector(),size+unreal.Vector(40,0,0)),unreal.GeometryScriptBooleanOperation.SUBTRACT)
  wall=boolean(outer,slab,unreal.GeometryScriptBooleanOperation.INTERSECTION)
  wall=boolean(wall,inner,unreal.GeometryScriptBooleanOperation.SUBTRACT)
  if index==3:
   heaven=next(a for a in aa if a.get_actor_label()=='COL_Heaven_OuterUnmineableBoundary');center,extent=heaven.get_actor_bounds(False)
   keepout=box(center-origin,extent*2-unreal.Vector(10))
   boolean(soil,keepout,unreal.GeometryScriptBooleanOperation.SUBTRACT);boolean(wall,keepout,unreal.GeometryScriptBooleanOperation.SUBTRACT)
  # Preserve the original end collars and their exact entrance/exit openings.
  old=unreal.MiningDesignerLibrary.get_campaign_rock_source(region.generated_shell)
  for side in [-1,1]:
   cap=boolean(clone(old),box(unreal.Vector(side*(size.x*.5+150),0,0),unreal.Vector(301,100000,100000)),unreal.GeometryScriptBooleanOperation.INTERSECTION)
   G.append_mesh(wall,cap,I)
  unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(wall,40000,unreal.GeometryScriptSimplifyMeshOptions())
  assert Q.get_is_closed_mesh(wall)
  unreal.GeometryScript_Normals.set_per_face_normals(wall)
  c=region.generated_shell.get_component_by_class(unreal.DynamicMeshComponent);c.get_dynamic_mesh().reset();G.append_mesh(c.get_dynamic_mesh(),wall,I);c.set_complex_as_simple_collision_enabled(True,True);c.update_collision(False)
  region.generated_shell.tags=list(region.generated_shell.tags)+['BM_ContourBedrock20m'];c.set_material(0,region.shell_material)
  buffer=A.spawn_actor_from_class(unreal.GeneratedMiningRock,origin);buffer.set_actor_label(f'MINE_{index:02}_Contour20m_DiggableBuffer');buffer.tags=[f'BM_Campaign_MainGame{index:02}','BM_ContourBedrock20m'];buffer.set_folder_path('MainGame/ContourBoundary')
  buffer.set_editor_property('rock_material',region.rock_material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);buffer.set_editor_property('hardness_level',region.rock_hardness)
  unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(soil,8000,unreal.GeometryScriptSimplifyMeshOptions())
  assert Q.get_is_closed_mesh(soil)
  assert unreal.MiningDesignerLibrary.set_campaign_rock_source(buffer,soil)
  record.update(status='built',wall_triangles=wall.get_triangle_count(),buffer_triangles=soil.get_triangle_count(),buffer_name=buffer.get_name(),buffer_chunks=len(buffer.chunks),original_rock=region.generated_rock.get_name(),shell=region.generated_shell.get_name());write_progress()
 assert L.save_current_level();report['passed']=True;write_progress()
except Exception:report['error']=traceback.format_exc();write_progress();unreal.log_error(report['error'])
finally:unreal.SystemLibrary.quit_editor()
