"""Restore existing clay material and exported 3D clods on MainGame V5."""
import unreal,json,sys,shutil,traceback,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
ART=ROOT/'Art/MainGame/RoutesV5/ClayRelief'
OUT=ROOT/'Saved/Validation/MainGameRoutesV5/ClayRelief';OUT.mkdir(parents=True,exist_ok=True)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
NEVER=unreal.PropertyAccessChangeNotifyMode.NEVER
report={'passed':False,'guides':[],'clods':[]}

def setp(a,k,v):a.set_editor_property(k,v,notify_mode=NEVER)
def assign(a,mat):
    setp(a,'rock_material',mat)
    for c in a.get_components_by_class(unreal.DynamicMeshComponent):c.set_material(0,mat)

try:
    assert not ed.get_game_world()
    backup=ROOT/'Saved/Backups'/('MainGame_ClayDebris_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/MainGame/Maps/Lvl_MainGame.umap',backup/'Lvl_MainGame.umap');report['backup']=str(backup)
    cfg=json.loads((ROOT/'Config/MainGameRooms.json').read_text(encoding='utf-8'))
    source_hashes={r['source']:hashlib.sha256((ROOT/'Content'/(r['source'][6:]+'.umap')).read_bytes()).hexdigest() for r in cfg['rooms']}
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame');assert levels.set_current_level_by_name('Lvl_MainGame')
    all_actors=list(actors.get_all_level_actors());hall=next(a for a in all_actors if isinstance(a,unreal.MiningExhibitHall))
    assert not hall.actor_has_tag('BM_RoutesV5ClayRelief'),'Already installed; inspect saved result instead of repeating.'
    manifest=json.loads((ART/'manifest.json').read_text(encoding='utf-8'));assert manifest['passed']
    assert json.loads((ART/'geometry_validation.json').read_text(encoding='utf-8'))['passed']
    clay=unreal.load_asset('/Game/MainGame/Materials/DirtTransition/MI_ClayBlockage');assert clay
    for i,item in enumerate(manifest['regions']):
        tag=f'BM_Campaign_MainGame{i+1:02}'
        region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(tag))
        setp(region,'guide_material',clay)
        for spec in item['plugs']:
            gate=region.generated_guide_layers[spec['index']];assert not gate.actor_has_tag('BM_BombOnly')
            assign(gate,clay)
            assert unreal.MiningDesignerLibrary.import_campaign_rock(gate,str(ART/spec['file']))
            gate.reset_rock();report['guides'].append({'actor':gate.get_actor_label(),'triangles':gate.triangle_count})
        for group in item['clod_meshes']:
            a=actors.spawn_actor_from_class(unreal.GeneratedMiningRock,region.get_actor_location())
            a.set_actor_label(f'MAIN_GAME_ClayWallClods_{i+1}_{group["guide"]}');setp(a,'tags',[tag,'BM_RoutesV5','BM_RoutesV5ClayRelief'])
            assign(a,clay);setp(a,'hardness_level',region.generated_rock.hardness_level)
            assert unreal.MiningDesignerLibrary.import_campaign_rock(a,str(ART/group['file']))
            a.reset_rock();report['clods'].append({'actor':a.get_actor_label(),'count':group['clods'],'triangles':a.triangle_count})
    for spec in cfg['generated_connections']:
        gate=unreal.load_object(None,spec['plug'])
        if gate and not gate.actor_has_tag('BM_BombOnly'):assign(gate,clay)
    # The downward museum soil also needs a real uneven face, not a flat cube.
    old=next(a for a in all_actors if a.actor_has_tag('BM_MuseumDownwardSoil'))
    a=actors.spawn_actor_from_class(unreal.GeneratedMiningRock,old.get_actor_location())
    setp(a,'tags',list(old.tags));setp(a,'hardness_level',old.hardness_level);assign(a,clay)
    assert unreal.MiningDesignerLibrary.build_campaign_soil(a,old.rock_size,unreal.Vector(15,15,35))
    field=api.get_field(hall);placements=list(field.get_editor_property('authored_placements'))
    for p in placements:
        p.set_editor_property('hosts',[a if host==old else host for host in p.hosts])
    setp(field,'authored_placements',placements)
    label=old.get_actor_label();assert actors.destroy_actor(old);a.set_actor_label(label)
    setp(hall,'tags',list(hall.tags)+['BM_RoutesV5ClayRelief'])
    valid=api.validate(api.get_field(hall),hall);assert valid['ok'],valid.get('errors')
    assert levels.save_current_level()
    assert all(hashlib.sha256((ROOT/'Content'/(p[6:]+'.umap')).read_bytes()).hexdigest()==h for p,h in source_hashes.items())
    report.update(passed=True,material=clay.get_path_name(),minerals=valid,source_maps_unchanged=True)
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'authoring.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
