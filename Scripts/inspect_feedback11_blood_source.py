import unreal,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/Feedback11/Blood';out.mkdir(parents=True,exist_ok=True)
ML=unreal.MaterialEditingLibrary;A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
r={}
try:
 mat=unreal.load_asset('/Game/BloodCirculation/Materials/MaterialInstance/BloodVessel/MI_Vein')
 base=mat.get_editor_property('parent')
 r['material']={'instance':mat.get_path_name(),'scalars':{str(n):ML.get_material_instance_scalar_parameter_value(mat,n) for n in ML.get_scalar_parameter_names(mat)},'textures':[]}
 for n in ML.get_material_expressions(base):
  row={'type':n.get_class().get_name(),'description':n.get_editor_property('desc')}
  if isinstance(n,unreal.MaterialExpressionTextureSample):
   t=n.get_editor_property('texture');row['texture']=t.get_path_name() if t else None
   row['sampler']=str(n.get_editor_property('sampler_type'));r['material']['textures'].append(row)
 r['meshes']=[]
 for path in ['/Game/BloodCirculation/Mesh/BloodVessel/SM_Vein','/Game/EndingEmergence/Meshes/SM_EE_BloodCirculationVeinSegment']:
  m=unreal.load_asset(path);d=unreal.DynamicMesh()
  unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(m,d,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD())
  _,uv,*_=d.get_mesh_per_vertex_u_vs(0) if hasattr(d,'get_mesh_per_vertex_u_vs') else getattr(d,next(a for a in dir(d) if a.replace('_','').lower()=='getmeshpervertexuvs'))(0)
  u=unreal.GeometryScript_List.convert_uv_list_to_array(uv)
  r['meshes'].append({'path':path,'materials':[m.get_material(i).get_path_name() for i in range(len(m.get_editor_property('static_materials')))],'bounds':str(m.get_bounds()),'triangles':m.get_num_triangles(0),'uv_range':[min(p.x for p in u),max(p.x for p in u),min(p.y for p in u),max(p.y for p in u)]})
 r['demos']={}
 for name in ['Demonstration1','Demonstration2','Demonstration3','Demonstration4','Demonstration5']:
  L.load_level('/Game/BloodCirculation/Maps/'+name);rows=[]
  for a in A.get_all_level_actors():
   for c in a.get_components_by_class(unreal.StaticMeshComponent):
    if c.static_mesh:rows.append({'actor':a.get_actor_label(),'mesh':c.static_mesh.get_name(),'location':str(c.get_world_location()),'materials':[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]})
  r['demos'][name]=rows
except Exception:r['error']=traceback.format_exc()
(out/'source.json').write_text(json.dumps(r,indent=2,ensure_ascii=False),encoding='utf8')
unreal.SystemLibrary.quit_editor()
