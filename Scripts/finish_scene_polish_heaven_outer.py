"""Cut the matching heaven clearance from the campaign's non-excavatable outer shell."""
import unreal,json,traceback
from pathlib import Path
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);lib=unreal.MiningDesignerLibrary
report={'passed':False,'outer_shells':[]};I=unreal.Transform()
try:
 assert L.load_level('/Game/MainGame/Maps/Lvl_MainGame');aa=list(A.get_all_level_actors())
 center=next(a for a in aa if a.get_actor_label()=='COL_Heaven_DiggableDomeShell').get_actor_location()
 tool=unreal.DynamicMesh();unreal.GeometryScript_Primitives.append_cylinder(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=center+unreal.Vector(0,0,2120)),radius=1980,height=4360,radial_steps=64,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 for a in aa:
  if not isinstance(a,unreal.DynamicMeshActor) or not a.actor_has_tag('BM_Campaign_MainGame03'):continue
  c,e=a.get_actor_bounds(False)
  if abs(c.x-center.x)>e.x+1980 or abs(c.y-center.y)>e.y+1980 or c.z+e.z<center.z-60 or c.z-e.z>center.z+4300:continue
  source=lib.get_campaign_rock_source(a);assert source;before=source.get_triangle_count()
  unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(source,a.get_actor_transform(),tool,I,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False))
  component=a.get_component_by_class(unreal.DynamicMeshComponent);component.get_dynamic_mesh().reset();unreal.GeometryScript_MeshEdits.append_mesh(component.get_dynamic_mesh(),source,I);component.update_collision(False)
  report['outer_shells'].append({'actor':a.get_actor_label(),'before':before,'after':source.get_triangle_count()})
 assert report['outer_shells'];assert L.save_current_level();report['passed']=True
except Exception:report['error']=traceback.format_exc()
(Path(unreal.Paths.project_dir())/'Saved/Validation/ScenePolish/heaven_outer.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
L.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
