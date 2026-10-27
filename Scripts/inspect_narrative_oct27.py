"""Read saved dialogue assets and current MainGame collectible placements; never save."""
import json, traceback, unreal
from pathlib import Path

out=Path(unreal.Paths.project_dir())/'Saved/Validation/NarrativeOct27'
out.mkdir(parents=True,exist_ok=True)
report={'passed':False,'placements':[],'segments':[],'triggers':[]}
try:
    for path in unreal.EditorAssetLibrary.list_assets('/Game/Subtitles',recursive=True,include_folder=False):
        if '/DA_' not in path:continue
        asset=unreal.load_asset(path)
        if isinstance(asset,unreal.SubtitleSegment):
            report['segments'].append({'asset':path,'id':str(asset.segment_id),'lines':[str(line.text) for line in asset.lines]})
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor,unreal.MiningCollectionField):
            for p in actor.authored_placements:
                report['placements'].append({'id':p.item_id,'area':str(p.area_tag),'position':list(p.transform.translation.to_tuple())})
        if isinstance(actor,unreal.SubtitleTrigger):
            report['triggers'].append({'label':actor.get_actor_label(),'id':str(actor.get_editor_property('segment_id')),'position':list(actor.get_actor_location().to_tuple())})
    report['passed']=True
except Exception:report['error']=traceback.format_exc()
finally:
    (out/'text-inventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
