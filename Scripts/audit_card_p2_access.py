import unreal,json
from pathlib import Path
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out=[]
for a in A.get_all_level_actors():
    if not isinstance(a,unreal.StaticMeshActor):continue
    p,e=a.get_actor_bounds(False)
    label=a.get_actor_label()
    if (p.x+e.x>9900 and p.x-e.x<11600 and p.y+e.y>-1300 and p.y-e.y<1300 and p.z+e.z>2900 and p.z-e.z<3300) or 'Landing light' in label:
        c=a.static_mesh_component
        out.append(dict(name=a.get_name(),label=label,position=str(a.get_actor_location()),bounds=[str(p),str(e)],collision=str(c.get_collision_enabled()),mesh=c.static_mesh.get_path_name() if c.static_mesh else None,visible=c.is_visible(),hidden_in_game=c.get_editor_property('hidden_in_game'),actor_hidden=a.get_editor_property('hidden')))
p=Path(unreal.Paths.project_dir())/'Saved/Validation/CardP2Flow/access-audit.json'
p.write_text(json.dumps(out,indent=2,ensure_ascii=False),encoding='utf-8')
ruler=next(a for a in A.get_all_level_actors() if a.get_actor_label()=='Study V09 / Desk ruler bridge')
t=ruler.get_actor_transform()
sweeps=[]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for y in [-35,0,35]:
    start=unreal.MathLibrary.transform_location(t,unreal.Vector(-45,y,50))+unreal.Vector(0,0,100)
    end=unreal.MathLibrary.transform_location(t,unreal.Vector(45,y,50))+unreal.Vector(0,0,100)
    for reverse in [False,True]:
        for complex_trace in [False,True]:
            result=unreal.SystemLibrary.capsule_trace_multi(world,end if reverse else start,start if reverse else end,34,88,unreal.TraceTypeQuery.ECC_VISIBILITY,complex_trace,[ruler],unreal.DrawDebugTrace.NONE,True)
            sweeps.append(dict(y=y,reverse=reverse,complex=complex_trace,result=[str(h.to_tuple()) for h in result] if result else []))
(p.parent/'ruler-sweeps.json').write_text(json.dumps(dict(transform=str(t),sweeps=sweeps),indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
