import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback11'
r={}
m=unreal.load_asset('/Game/PopupClose/Maps/_GENERATED/Lenovo/office_doors_Submesh_704FAA0D')
r['door_materials']=[dict(slot=str(x.material_slot_name),material=x.material_interface.get_path_name() if x.material_interface else '') for x in m.static_materials]
dm=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(m,dm,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
r['mesh_methods']=[x for x in dir(unreal.GeometryScript_MeshQueries) if 'triangle' in x or 'bound' in x]
r['copy_options']=unreal.GeometryScriptCopyMeshFromAssetOptions.__doc__
r['material_query_doc']=unreal.GeometryScript_Materials.get_all_triangle_material_i_ds.__doc__
r['delete_doc']=unreal.GeometryScript_Materials.delete_triangles_by_material_id.__doc__
r['triangle_materials']=str(unreal.GeometryScript_Materials.get_all_triangle_material_i_ds(dm))
r['triangles']=dm.get_triangle_count()
r['section_materials']=str(unreal.GeometryScript_AssetUtils.get_section_material_list_from_static_mesh(m,unreal.GeometryScriptMeshReadLOD()))
r['histogram']={}
for i in range(dm.get_triangle_count()):
 mid,valid=unreal.GeometryScript_Materials.get_triangle_material_id(dm,i)
 if valid:r['histogram'][str(mid)]=r['histogram'].get(str(mid),0)+1
r['material_methods']=[x for x in dir(unreal.GeometryScript_Materials) if 'material' in x]
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame')
r['near_soil']=[]
for a in A.get_all_level_actors():
 p,e=a.get_actor_bounds(False)
 if abs(p.x-47200)<e.x+650 and abs(p.y-11750)<e.y+300 and abs(p.z-15500)<e.z+400:
  r['near_soil'].append(dict(name=a.get_actor_label(),cls=a.get_class().get_name(),center=[p.x,p.y,p.z],extent=[e.x,e.y,e.z]))
assert L.load_level('/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo')
d=next(a for a in A.get_all_level_actors() if isinstance(a,unreal.EndingEmergence))
r['galaxies']=[]
for c in d.get_components_by_class(unreal.InstancedStaticMeshComponent):
 if c.get_name().startswith('DenseCellGalaxies'):
  r['galaxies'].append(dict(name=c.get_name(),count=c.get_instance_count(),mat=c.get_material(0).get_path_name(),visible=c.is_visible(),hidden=c.get_editor_property('hidden_in_game'),reveal=c.get_material(0).get_scalar_parameter_value('Reveal')))
(out/'geometry.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
