"""Read saved MainGame placements and branch geometry; do not modify assets."""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/Validation/EmptyDeadEnds';out.mkdir(parents=True,exist_ok=True)
report={}
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
    field=next(a for a in actors if isinstance(a,unreal.MiningCollectionField) and a.authored_placements)
    report['collectibles']=[dict(id=p.item_id,area=str(p.area_tag),position=list(p.transform.translation.to_tuple()),clue=list(p.clue_transform.translation.to_tuple()),approach=list(p.approach_transform.translation.to_tuple()),host=p.host_rock.get_actor_label() if p.host_rock else None) for p in field.authored_placements]
    report['regions']=[]
    for i,stage in enumerate(['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']):
        region=next(a for a in actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}'))
        origin=region.get_actor_location()+region.cutters[0].local_transform.translation
        paths=json.loads((root/'Art/MainGame/RoutesV6'/stage/'centerlines.json').read_text())
        report['regions'].append(dict(number=i+1,stage=stage,origin=list(origin.to_tuple()),paths=[dict(name=p['name'],points=[[origin.x+q[0]*100,origin.y-q[1]*100,origin.z+q[2]*100] for q in p['points']]) for p in paths]))
    report['blockers']=[dict(name=a.get_actor_label(),position=list(a.get_actor_location().to_tuple()),tags=[str(t) for t in a.tags]) for a in actors if any(word in a.get_actor_label().lower() for word in ['bomb','branch','dead','heaven','pigeon'])]
    report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
(out/'saved-map.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
