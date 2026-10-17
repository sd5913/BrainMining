import unreal, json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.get_current_level().get_outer().get_name()=='Lvl_EchoMazeToCards_P2'
def v(p):return [round(p.x,3),round(p.y,3),round(p.z,3)]
rows=[]
for a in A.get_all_level_actors():
 t=a.get_actor_transform(); o,e=a.get_actor_bounds(False)
 cs=[]
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  mesh=c.static_mesh
  cs.append(dict(name=c.get_name(),mesh=mesh.get_path_name() if mesh else None,loc=v(c.get_editor_property('relative_location')),scale=v(c.get_editor_property('relative_scale3d')),visible=c.is_visible(),materials=[m.get_path_name() if m else None for m in c.get_materials()]))
 rows.append(dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),path=a.get_path_name(),loc=v(t.translation),rotation=[a.get_actor_rotation().pitch,a.get_actor_rotation().yaw,a.get_actor_rotation().roll],scale=v(t.scale3d),origin=v(o),extent=v(e),tags=[str(x) for x in a.tags],components=cs))
root=Path(unreal.Paths.project_saved_dir())/'Automation/CountrysideP2';root.mkdir(parents=True,exist_ok=True)
(root/'before.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
ar=unreal.AssetRegistryHelpers.get_asset_registry();ar.scan_paths_synchronous(['/Game/Countryside_Interior'],True)
meshrows=[]
for ad in ar.get_assets_by_path('/Game/Countryside_Interior/Meshes',recursive=True):
 m=ad.get_asset()
 if isinstance(m,unreal.StaticMesh):
  b=m.get_bounds();meshrows.append(dict(name=m.get_name(),path=m.get_path_name(),origin=v(b.origin),extent=v(b.box_extent),materials=[x.material_interface.get_path_name() if x.material_interface else None for x in m.static_materials]))
(root/'meshes.json').write_text(json.dumps(meshrows,indent=2),encoding='utf-8')
unreal.log('COUNTRYSIDE_P2_INSPECTION_READY')
