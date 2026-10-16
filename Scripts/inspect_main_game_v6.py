import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRoutesV6';OUT.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();report=[]
for a in actors:
    center,extent=a.get_actor_bounds(False)
    near_shaft=abs(center.x+32680)<1200+extent.x and abs(center.y)<400+extent.y and 11000-extent.z<center.z<12500+extent.z
    if not (a.get_actor_label().startswith(('STARTROOM_','MAIN_GAME_Clay')) or isinstance(a,unreal.MiningStartRoom) or (near_shaft and isinstance(a,unreal.StaticMeshActor))):continue
    report.append(dict(label=a.get_actor_label(),path=a.get_path_name(),type=a.get_class().get_name(),location=list(a.get_actor_location().to_tuple()),center=list(center.to_tuple()),extent=list(extent.to_tuple()),hidden=a.get_editor_property('hidden'),components=[dict(name=c.get_name(),material=str(c.get_material(0))) for c in a.get_components_by_class(unreal.MeshComponent)]))
(OUT/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
