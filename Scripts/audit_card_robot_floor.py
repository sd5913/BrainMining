import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/CardRobot'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();rows=[]
for a in actors:
    for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
        rows.append(dict(actor=a.get_name(),label=a.get_actor_label(),component=c.get_name(),skeletal=True,visible=c.is_visible(),location=list(c.get_world_location().to_tuple())))
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh=c.static_mesh
        if not mesh:continue
        p=c.get_world_location()
        if (p-unreal.Vector(9950,6550,600)).length()<4500 or any(n in mesh.get_name().lower() for n in ['cat','toy','rabbit','doll']):
            rows.append(dict(actor=a.get_name(),label=a.get_actor_label(),component=c.get_name(),mesh=mesh.get_path_name(),visible=c.is_visible(),location=list(p.to_tuple())))
(out/'floor-audit.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
