"""Swap only the saved Journey source reference; preserve all host mining geometry."""
import unreal,json,shutil,hashlib,traceback
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/CardRobot';out.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
target='/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2';main='/Game/MainGame/Maps/Lvl_MainGame'
report=dict(passed=False)
try:
    backup=root/'Saved/Backups'/('MainGame-CardP2-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(root/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'MainGame.umap')
    source=root/'Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2.umap';digest=hashlib.sha256(source.read_bytes()).hexdigest()
    assert levels.load_level(main)
    hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall));areas=list(hall.areas)
    index=next(i for i,a in enumerate(areas) if str(a.tag)=='BM_Campaign_MainGameJourney')
    def snapshot():
        return sorted((a.get_name(),a.get_class().get_name(),a.get_actor_location().to_tuple(),a.get_actor_rotation().to_tuple(),a.get_actor_scale3d().to_tuple()) for a in actors.get_all_level_actors() if a.get_level()==hall.get_level())
    before=snapshot();old=areas[index].source_level;transform=old.get_editor_property('level_transform')
    oldpath=old.get_loaded_level().get_path_name()
    if not oldpath.startswith(target+'.'):
        assert unreal.EditorLevelUtils.remove_level_from_world(old.get_loaded_level())
        stream=unreal.EditorLevelUtils.add_level_to_world_with_transform(ed.get_editor_world(),target,unreal.LevelStreamingAlwaysLoaded,transform)
        assert stream;areas[index].set_editor_property('source_level',stream);hall.set_editor_property('areas',areas)
    assert snapshot()==before,'Persistent actors changed while replacing the source reference'
    assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),main)
    assert hashlib.sha256(source.read_bytes()).hexdigest()==digest,'Source map must not be resaved with its MainGame transform'
    specpath=root/'Config/MainGameRooms.json';spec=json.loads(specpath.read_text(encoding='utf-8'))
    next(a for a in spec['rooms'] if a['id']=='Journey')['source']=target
    specpath.write_text(json.dumps(spec,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report.update(passed=True,backup=str(backup),old_source=oldpath,new_source=target,origin=str(transform.translation),host_actors_preserved=len(before),source_sha256=digest,area_index=index)
except Exception:report['error']=traceback.format_exc()
(out/'main-game-link.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.quit_editor()
