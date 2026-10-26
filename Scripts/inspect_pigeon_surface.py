import json, traceback
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/PigeonSurface';out.mkdir(parents=True,exist_ok=True)
r={}
try:
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
 p=next(p for p in field.authored_placements if p.item_id==3);host=p.host_rock
 r['placement']={k:str(getattr(p,k)) for k in ['transform','clue_transform','approach_transform']}
 r['host']={'name':host.get_name(),'transform':str(host.get_actor_transform()),'material':host.rock_material.get_path_name()}
 mesh=unreal.MiningDesignerLibrary.get_campaign_rock_source(host);frame=host.get_actor_transform()
 r['triangles']=[]
 for i in range(mesh.get_triangle_count()):
  valid,*vs=unreal.GeometryScript_MeshQueries.get_triangle_positions(mesh,i)
  if valid:r['triangles'].append([list(frame.transform_location(v).to_tuple()) for v in vs])
 r['nearby']=[]
 for a in actors:
  if not isinstance(a,unreal.DiggableRock):continue
  center,extent=a.get_actor_bounds(False)
  q=p.clue_transform.translation-center
  if all(abs(getattr(q,k))<getattr(extent,k)+200 for k in ['x','y','z']):r['nearby'].append({'name':a.get_name(),'label':a.get_actor_label(),'material':a.rock_material.get_path_name() if a.rock_material else '', 'center':list(center.to_tuple()),'extent':list(extent.to_tuple())})
 r['materials']=[]
 paths=set(x['material'] for x in r['nearby']);paths.add(host.rock_material.get_path_name())
 for path in paths:
  m=unreal.load_asset(path);entry={'path':path,'instances':[]}
  while isinstance(m,unreal.MaterialInstanceConstant):
   entry['instances'].append({'name':m.get_path_name(),'scalar':str(m.get_editor_property('scalar_parameter_values')),'vector':str(m.get_editor_property('vector_parameter_values'))});m=m.get_editor_property('parent')
  entry['parent']=m.get_path_name();entry['custom']=[n.get_editor_property('code') for n in unreal.ObjectIterator(unreal.MaterialExpressionCustom) if n.get_outer()==m]
  entry['expressions']=[n.get_class().get_name()+': '+str(n.get_editor_property('desc')) for n in unreal.MaterialEditingLibrary.get_material_expressions(m)]
  r['materials'].append(entry)
except:r['error']=traceback.format_exc()
finally:
 (out/'inspection.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf8');unreal.SystemLibrary.quit_editor()
