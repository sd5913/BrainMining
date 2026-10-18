import unreal,json,sys,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
from main_game_collection_dressing import dress_heaven
OUT=ROOT/'Saved/Validation/MainGameCollections';cfg=json.loads((ROOT/'Config/MainGameCollections.json').read_text());auth=json.loads((OUT/'authoring.json').read_text());report={'passed':False}
try:
    assert auth['passed']
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert levels.load_level(cfg['preview_map'])
    frame=unreal.Transform(location=unreal.Vector(*auth['heaven']['frame_position']),rotation=unreal.Rotator(yaw=auth['heaven']['yaw']))
    report.update(dress_heaven(ed.get_editor_world(),cfg['ownership_tag'],frame))
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and cfg['ownership_tag'] in [str(t) for t in a.tags])
    chair=next(a for a in actors if a.get_actor_label()==cfg['cassette']['chair_label'] and 'PopupClose1' in a.get_level().get_path_name())
    placements=list(field.get_editor_property('authored_placements'))
    for p in placements:
        if p.item_id==5:
            t=p.transform;t.rotation=unreal.Rotator(yaw=chair.get_actor_rotation().yaw+cfg['cassette']['yaw_offset']).quaternion();p.transform=t
            report['cassette_rotation']=list(t.rotation.rotator().to_tuple())
    field.set_editor_property('authored_placements',placements)
    assert levels.save_current_level()
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in auth['source_hashes'].items())
    report['passed']=True
    auth['config_sha256']=hashlib.sha256((ROOT/'Config/MainGameCollections.json').read_bytes()).hexdigest()
    (OUT/'authoring.json').write_text(json.dumps(auth,ensure_ascii=False,indent=2),encoding='utf-8')
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'dressing.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
