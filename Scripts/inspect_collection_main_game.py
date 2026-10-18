"""Read-only MainGame anchors; never save a source level."""
import unreal, json, traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/MainGameCollections'; OUT.mkdir(parents=True,exist_ok=True)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(v): return list(v.to_tuple())
try:
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    rows=[]
    for a in api.get_all_level_actors():
        p,e=a.get_actor_bounds(False)
        row=dict(name=a.get_name(),label=a.get_actor_label(),type=a.get_class().get_name(),position=xyz(a.get_actor_location()),rotation=str(a.get_actor_rotation()),bounds=[xyz(p),xyz(e)],level=a.get_level().get_path_name(),tags=[str(t) for t in a.tags])
        if isinstance(a,unreal.MiningExhibitHall):
            row['areas']=[dict(tag=str(x.tag),kind=str(x.kind),origin=xyz(x.origin),entry=xyz(x.entry.translation),exit=xyz(x.exit),route=[xyz(v) for v in x.map_route],bounds=[xyz(x.mining_bounds.min),xyz(x.mining_bounds.max)],source=str(x.source_level)) for x in a.areas]
        if isinstance(a,unreal.MiningExternalRegion):
            row.update(rock=a.generated_rock.get_name(),shell=a.generated_shell.get_name(),guides=[g.get_name() for g in a.generated_guide_layers])
        if isinstance(a,unreal.StaticMeshActor):
            row['mesh']=str(a.static_mesh_component.static_mesh)
        rows.append(row)
    (OUT/'anchors.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
except Exception:
    (OUT/'inspection_error.txt').write_text(traceback.format_exc());raise
finally:
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
