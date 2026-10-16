"""Reload and inspect linked map references and immutable source maps."""
import unreal,json,hashlib,traceback,sys
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameRooms';report={'passed':False,'checks':[]}
def check(ok,msg):assert ok,msg;report['checks'].append(msg)
try:
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert not ed.get_game_world();assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    actors=list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors());hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall))
    cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text());baseline=json.loads((OUT/'before.json').read_text())
    check(len(hall.areas)==7 and hall.continuous_campaign and hall.linear_campaign,'Saved seven-section continuous campaign')
    for index,spec in zip([1,3,5],cfg['rooms']):
        area=hall.areas[index];stream=area.source_level;loaded=stream.get_loaded_level()
        check(isinstance(stream,unreal.LevelStreamingAlwaysLoaded) and loaded is not None,'Always-loaded original map reference: '+spec['id'])
        check(loaded.get_path_name().startswith(spec['source']+'.'),'Map dependency references the requested original: '+spec['id'])
        check(len([a for a in actors if a.get_level()==loaded])==len(baseline['sources'][spec['source']]),'Full authored source actor count preserved: '+spec['id'])
        check(stream.get_editor_property('level_transform').translation.distance(unreal.Vector(*spec['origin']))<.01,'Saved source transform: '+spec['id'])
        check(area.progress_gate is not None,'Room flow completion gate saved: '+spec['id'])
    check(not [a for a in actors if a.get_level()==hall.get_level() and isinstance(a,(unreal.PopupCloseStage,unreal.WenSiMirrorPuzzle,unreal.EchoPassageStage,unreal.CardRoomStage))],'Room gameplay actors are referenced rather than copied into persistent level')
    check(len([a for a in actors if isinstance(a,unreal.MiningExternalRegion)])==4,'Only the four real excavation areas remain')
    for source,digest in baseline['source_hashes'].items():check(hashlib.sha256((ROOT/'Content'/(source[6:]+'.umap')).read_bytes()).hexdigest()==digest,'Original source map unchanged: '+source)
    for spec in cfg['generated_connections']:
        plug=next(a for a in actors if a.get_actor_label()=='LINK_'+spec['id']+'_DiggableSoil')
        check(isinstance(plug,unreal.DiggableRock) and plug.triangle_count>0 and plug.rock_material.get_name()=='MI_ClayBlockage','Saved mineable moss-soil interface: '+spec['id'])
    report['passed']=True
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'saved_state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('ROOMS_SAVED '+json.dumps(report,ensure_ascii=False))
