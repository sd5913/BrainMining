"""Match old outer mine seals to the new natural interface ramps."""
import unreal,json,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'));import main_game_room_entrances_geometry as g
assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall));before=json.loads((ROOT/'Saved/Validation/MainGameRoutesV2/before.json').read_text());areas=list(hall.areas)
for s in cfg['generated_connections']:
 mouth=cfg['mine_origins'][s['mine']][0]+(-6500 if s['side']=='entry' else 6500);prefix='LINK_'+s['tag']+'_'+str(round(mouth))+'_Collar_'
 region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(s['tag']));shell=region.generated_shell;c=shell.get_component_by_class(unreal.DynamicMeshComponent);mesh=c.get_dynamic_mesh();tool=unreal.DynamicMesh();floor=min(g.floor_at(s,mouth-400),g.floor_at(s,mouth+400))
 world=unreal.Vector(mouth,s['a'][1],floor+500);local=unreal.MathLibrary.inverse_transform_location(shell.get_actor_transform(),world)
 unreal.GeometryScript_Primitives.append_box(tool,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(location=local),dimension_x=800,dimension_y=1100,dimension_z=1500,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
 unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh,unreal.Transform(),tool,unreal.Transform(),unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions(fill_holes=True,simplify_output=False));assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(mesh);c.update_collision(False)
 for a in actors:
  label=a.get_actor_label()
  if not label.startswith(prefix):continue
  p,e=a.get_actor_bounds(False);lo=p-e;hi=p+e;floors=[g.floor_at(s,lo.x),g.floor_at(s,hi.x)]
  if label.endswith('_Below'):hi.z=min(floors)-s['backing_depth']-12
  elif label.endswith('_Above'):lo.z=max(floors)+950
  else:continue
  a.set_actor_location((lo+hi)*.5,False,True);a.set_actor_scale3d((hi-lo)/100)
for i in range(1,4):
 area=areas[i*2];route=list(area.map_route);route[0]=unreal.Vector(*before['areas'][i]['start'])+unreal.Vector(*cfg['mine_deltas'][i]);area.set_editor_property('map_route',route)
hall.set_editor_property('areas',areas,notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER);assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();unreal.log('NATURAL_CAVE_BACKING_SAVED')
