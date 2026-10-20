"""Copy only Demo2 cell-room art into the authored production map.
Run after rebuilding C++; never regenerates the director or city scenery.
"""
import unreal,json,hashlib
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/EndingArt'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
source='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo2';target='/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
assert levels.load_level(source)
actors=api.get_all_level_actors();rows=[];door=None
for a in actors:
 if not isinstance(a,unreal.StaticMeshActor):continue
 p=a.get_actor_location()
 if not (-1000<p.x<1800 and abs(p.y)<1500 and -500<p.z<2000):continue
 c=a.static_mesh_component
 if not c.static_mesh:continue
 row=dict(name=a.get_actor_label(),transform=a.get_actor_transform(),mesh=c.static_mesh,materials=[c.get_material(i) for i in range(c.get_num_materials())],shadow=c.get_editor_property('cast_shadow'),collision=c.get_collision_enabled())
 if a.get_actor_label()=='SM_Thrombus3':door=row
 else:rows.append(row)
assert door and len(rows)>25,(door,len(rows))
# Extract the new door's real surface, close only open boundary holes, and bake
# its artist-authored world pose into an unscaled diggable actor.
mesh=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(door['mesh'],mesh,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
weld=unreal.GeometryScriptWeldEdgesOptions();weld.tolerance=.001;mesh.weld_mesh_edges(weld)
_,filled,failed=mesh.fill_all_mesh_holes(unreal.GeometryScriptFillHolesOptions())
assert mesh.get_is_closed_mesh(), f'Door remains open after filling {filled}; failed {failed}'
mesh.compact_mesh();mesh.transform_mesh(door['transform'])
_,positions,_=mesh.get_all_vertex_positions(False);vs=unreal.GeometryScript_List.convert_vector_list_to_array(positions)
mins=[min(getattr(v,k) for v in vs) for k in ['x','y','z']];maxs=[max(getattr(v,k) for v in vs) for k in ['x','y','z']]
center=unreal.Vector(*[(a+b)/2 for a,b in zip(mins,maxs)]);size=unreal.Vector(*[b-a for a,b in zip(mins,maxs)])
mesh.transform_mesh(unreal.Transform(location=unreal.Vector(-center.x,-center.y,-center.z)));mesh.recompute_normals(unreal.GeometryScriptCalculateNormalsOptions())
assert levels.load_level(target)
all_actors=api.get_all_level_actors();director=next(a for a in all_actors if isinstance(a,unreal.EndingEmergence))
assert director.get_editor_property('bEditableScenery') and not director.actor_has_tag('EndingDemo2CellInstalled')
components=director.get_components_by_class(unreal.SceneComponent);cell_root=next(c for c in components if c.get_name().startswith('01_PlayableCellsBloodBrain'))
old_cell=[a for a in all_actors if a.actor_has_tag('EndingEditableCell') and isinstance(a,unreal.StaticMeshActor)]
old_gate=list(director.gates);assert len(old_gate)==1 and old_gate[0]
old_gate_names=[a.get_name() for a in old_gate]
# Record everything outside the replacement scope to prove authored city/cameras
# and later chapters are retained through the edit.
def signature(a):return [a.get_path_name(),str(a.get_actor_transform())]
keep=[signature(a) for a in all_actors if a not in old_cell and a not in old_gate and a!=director]
created=[]
for row in rows:
 a=api.spawn_actor_from_class(unreal.StaticMeshActor,row['transform'].translation)
 c=a.static_mesh_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_static_mesh(row['mesh'])
 for i,m in enumerate(row['materials']):c.set_material(i,m)
 c.set_editor_property('cast_shadow',row['shadow']);c.set_editor_property('generate_overlap_events',False)
 # The cutaway organelles do not use broad convex hulls that would block free
 # floating in empty parts of their bounds; the enclosing membrane retains collision.
 shell='EllipsoidalCellInterior' in row['name']
 c.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if shell else unreal.CollisionEnabled.NO_COLLISION)
 c.set_collision_profile_name('BlockAll' if shell else 'NoCollision')
 a.set_actor_transform(row['transform'],False,False)
 a.attach_to_component(cell_root,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
 a.set_actor_label('EDIT_细胞房_Demo2_'+row['name']);a.set_folder_path('Ending_Edit/01_Cell/Demo2')
 a.tags=['EndingEditableScenery','EndingEditableCell','EndingDemo2Cell']
 if not shell and row['name']!='cell':a.tags=list(a.tags)+['EndingDemo2Organelle']
 c.component_tags=list(c.component_tags)+['EndingCellVisual']
 created.append(a)
new_gate=api.spawn_actor_from_class(unreal.DiggableRock,center)
new_gate.set_actor_label('EDIT_细胞房_Demo2_可挖血栓门');new_gate.set_folder_path('Ending_Edit/01_Cell/Demo2')
new_gate.rock_size=size;new_gate.rock_material=door['materials'][0];new_gate.hardness_level=old_gate[0].hardness_level
new_gate.tags=['EmergenceEndingGenerated','EndingDemo2CellGate'];new_gate.attach_to_component(cell_root,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
new_gate.bake_authored_mesh(mesh)
assert new_gate.get_editor_property('AuthoredRestMesh') and new_gate.triangle_count>4000
new_gate.rock_mesh.set_material(0,door['materials'][0]);director.gates[0]=new_gate;assert director.gates[0]==new_gate
for a in old_cell+old_gate:assert api.destroy_actor(a)
director.tags=list(director.tags)+['EndingDemo2CellInstalled']
after=api.get_all_level_actors();assert all(signature(a) in keep for a in after if a not in created and a!=new_gate and a!=director)
# Restore the authored edit-time cell visibility after attaching new roots.
for a in created:a.static_mesh_component.set_visibility(True)
assert levels.save_current_level()
(out/'cell_import.json').write_text(json.dumps(dict(source=source,target=target,imported=len(created),removed_old_cell=len(old_cell),removed_old_gate=old_gate_names,gate_mesh=door['mesh'].get_path_name(),gate_pos=[center.x,center.y,center.z],gate_size=[size.x,size.y,size.z],gate_triangles=new_gate.triangle_count,gate_filled_holes=filled,other_actors_retained=len(keep),actors=[a.get_actor_label() for a in created]),ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
