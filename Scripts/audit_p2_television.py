import unreal,json
from pathlib import Path
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
w=E.get_game_world() or E.get_editor_world()
rows=[]
for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
 p,e=a.get_actor_bounds(False)
 if ('MiningRoomEscape' in a.get_class().get_name() or
     (p.x+e.x>22400 and p.x-e.x<24500 and p.y+e.y>10800 and p.y-e.y<13000 and p.z+e.z>5800 and p.z-e.z<8500)):
  rows.append(dict(name=a.get_name(),label=a.get_actor_label(),cls=a.get_class().get_name(),center=str(p),extent=str(e),meshes=[dict(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,collision=str(c.get_collision_enabled())) for c in a.get_components_by_class(unreal.StaticMeshComponent)]))
Path(unreal.Paths.project_dir()+'Saved/Validation/IntroP2Office/tv_audit_'+('runtime' if E.get_game_world() else 'editor')+'.json').write_text(json.dumps(rows,indent=2),encoding='utf8')

