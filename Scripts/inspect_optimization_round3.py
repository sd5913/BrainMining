import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Saved/Validation/OptimizationRound3';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
report=[]
for a in actors.get_all_level_actors():
    if 'CrystalMotes' in a.get_actor_label() or a.actor_has_tag('BM_ParkourPlatform') or 'OfficeExit_BombOnly' in a.get_actor_label() or (isinstance(a,unreal.WenSiMirrorExhibit) and a.get_editor_property('kind')==7):
        row=dict(label=a.get_actor_label(),cls=a.get_class().get_name(),location=list(a.get_actor_location().to_tuple()),scale=list(a.get_actor_scale3d().to_tuple()),bounds=str(a.get_actor_bounds(False)))
        if isinstance(a,unreal.NiagaraActor):
            c=a.get_component_by_class(unreal.NiagaraComponent);asset=c.get_asset()
            row.update(asset=asset.get_path_name(),system_api=[x for x in dir(asset) if 'param' in x.lower() or 'emit' in x.lower()],component_api=[x for x in dir(c) if 'param' in x.lower() or 'bound' in x.lower()])
        report.append(row)
(out/'inspection.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
levels.load_level('/Engine/Maps/Entry');unreal.SystemLibrary.quit_editor()
