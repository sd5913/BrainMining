"""Join the old heaven enclosure to the current 20 m mining envelope, and refresh the actual campaign museum copy."""
import unreal,json,traceback,shutil,time
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/PlayerFeedback26';L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);I=unreal.Transform();G=unreal.GeometryScript_MeshEdits;Q=unreal.GeometryScript_MeshQueries;B=unreal.GeometryScript_MeshBooleans;r={}
def save(): (out/'heaven-museum.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
def clone(src):m=unreal.DynamicMesh();G.append_mesh(m,src,I);return m
def cut(source,frame,tool,toolframe):
 original=clone(source)
 for shift in [0,.137,.413]:
  source.reset();G.append_mesh(source,original,I);t=unreal.Transform(location=toolframe.translation+unreal.Vector(shift,shift*.71,shift*.37))
  B.apply_mesh_boolean(source,frame,tool,t,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
  if Q.get_is_closed_mesh(source):return
  unreal.GeometryScript_MeshRepair.weld_mesh_edges(source,unreal.GeometryScriptWeldEdgesOptions(tolerance=.1))
  if Q.get_is_closed_mesh(source):return
 # Old boundary cuts already contain coincident edges. Reconstruct only this exterior shell.
 repair=unreal.GeometryScript3DGridParameters(size_method=unreal.GeometryScriptGridSizingMethod.GRID_CELL_SIZE,grid_cell_size=40)
 unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_solidify(source,unreal.GeometryScriptSolidifyOptions(grid_parameters=repair,surface_search_steps=6))
 assert source.get_triangle_count()>0 and Q.get_is_closed_mesh(source),'Heaven cut produced open mesh'
 r['repaired_boundary']=True
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(A.get_all_level_actors());region=next(a for a in aa if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_Campaign_MainGame03'))
 backup=root/'Saved/Backups'/('BeforeFeedback26Heaven-'+str(int(time.time()))+'.umap');shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup)
 # The gray enclosure was last opened against RoutesV5. Reconcile with the assigned V6 cutters.
 core=unreal.DynamicMesh()
 for cutter in region.cutters:
  if not cutter.mesh or not cutter.enabled:continue
  tool=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(cutter.mesh,tool,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0));G.append_mesh(core,tool,cutter.local_transform)
 inner=clone(core);grid=unreal.GeometryScript3DGridParameters(size_method=unreal.GeometryScriptGridSizingMethod.GRID_CELL_SIZE,grid_cell_size=75)
 unreal.GeometryScript_MeshVoxelProcessing.apply_mesh_morphology(inner,unreal.GeometryScriptMorphologyOptions(sdf_grid_parameters=grid,operation=unreal.GeometryScriptMorphologicalOpType.DILATE,distance=2000));assert Q.get_is_closed_mesh(inner)
 r['envelope_triangles']=inner.get_triangle_count();save()
 boundary=next(a for a in aa if a.get_actor_label()=='COL_Heaven_OuterUnmineableBoundary');source=unreal.MiningDesignerLibrary.get_campaign_rock_source(boundary);r['before_triangles']=source.get_triangle_count();r['before_closed']=Q.get_is_closed_mesh(source)
 cut(source,boundary.get_actor_transform(),inner,region.get_actor_transform());assert Q.get_is_closed_mesh(source)
 unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(source,20000,unreal.GeometryScriptSimplifyMeshOptions());assert Q.get_is_closed_mesh(source)
 unreal.GeometryScript_Normals.set_per_face_normals(source)
 comp=boundary.get_component_by_class(unreal.DynamicMeshComponent);comp.get_dynamic_mesh().reset();G.append_mesh(comp.get_dynamic_mesh(),source,I);comp.set_material(0,region.shell_material);comp.set_complex_as_simple_collision_enabled(True,True);comp.update_collision(False);r['after_triangles']=source.get_triangle_count()
 # Existing excavatable backing retains its editable solid, with the surrounding mine's rock material.
 buffer=next(a for a in aa if a.get_actor_label()=='COL_Heaven_OuterDiggableBuffer');buffer.set_editor_property('rock_material',region.rock_material,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
 for comp in buffer.get_components_by_class(unreal.DynamicMeshComponent):comp.set_material(0,region.rock_material)
 assert L.save_current_level();r['heaven_saved']=True;save()
 path='/Game/WenSi/Maps/Lvl_WenSi_MirrorMuseum1';shutil.copy2(root/'Content/WenSi/Maps/Lvl_WenSi_MirrorMuseum1.umap',root/'Saved/Backups'/('BeforeFeedback26Museum1-'+str(int(time.time()))+'.umap'))
 assert L.load_level(path);unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'WenSi.UpgradeBalance')
 r['museum_markers']=[a.get_actor_label() for a in A.get_all_level_actors() if a.get_actor_label().startswith(('HopperAperture','ContactTerminal','CircuitLightning'))];assert len(r['museum_markers'])==9
 r['passed']=True
except:r['error']=traceback.format_exc();unreal.log_error(r['error'])
finally:save();unreal.SystemLibrary.quit_editor()
