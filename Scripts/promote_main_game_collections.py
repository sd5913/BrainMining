"""Promote a fresh collection preview, refusing to overwrite newer source terrain."""
import unreal, json, hashlib, shutil, traceback
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/Validation/MainGameCollections'
cfg=json.loads((ROOT/'Config/MainGameCollections.json').read_text())
auth=json.loads((OUT/'authoring.json').read_text())
report={'passed':False}
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
try:
    assert auth['passed'] and auth['config_sha256']==sha(ROOT/'Config/MainGameCollections.json')
    for p,h in auth['source_hashes'].items():assert sha(p)==h,'Source changed since preview: '+p
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not ed.get_game_world()
    source=ROOT/'Content'/(cfg['source_map'][6:]+'.umap')
    backup=ROOT/'Saved/Backups'/('MainGame_BeforeCollections_'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
    backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
    assert levels.load_level(cfg['preview_map'])
    fields=unreal.GameplayStatics.get_all_actors_of_class(ed.get_editor_world(),unreal.MiningCollectionField)
    assert len(fields)==1 and {p.item_id for p in fields[0].authored_placements}==set(range(11))
    fields=[]
    assert unreal.EditorLoadingAndSavingUtils.save_map(ed.get_editor_world(),cfg['source_map'])
    assert levels.load_level(cfg['source_map'])
    fields=unreal.GameplayStatics.get_all_actors_of_class(ed.get_editor_world(),unreal.MiningCollectionField)
    assert len(fields)==1 and len(fields[0].authored_placements)==11
    for p,h in auth['source_hashes'].items():
        if Path(p)!=source:assert sha(p)==h,'Shared room changed: '+p
    report.update(passed=True,map=cfg['source_map'],backup=str(backup),source_hash_before=sha(backup),source_hash_after=sha(source),placements=11,ribbon='Existing TV stage')
except Exception:report['error']=traceback.format_exc()
(OUT/'promotion.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('COLLECTION_PROMOTION '+json.dumps(report))
unreal.SystemLibrary.quit_editor()
