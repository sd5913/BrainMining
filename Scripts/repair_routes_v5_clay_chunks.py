import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();ART=ROOT/'Art/MainGame/RoutesV5/ClayRelief'
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert json.loads((ART/'geometry_validation.json').read_text())['passed']
assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
world=ed.get_editor_world();all_actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)
rocks=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.GeneratedMiningRock)
report=[]
for i,item in enumerate(json.loads((ART/'manifest.json').read_text())['regions']):
    region=next(a for a in all_actors if a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}'))
    for spec in item['plugs']:
        gate=region.generated_guide_layers[spec['index']]
        assert unreal.MiningDesignerLibrary.import_campaign_rock(gate,str(ART/spec['file']))
        assert gate.get_editor_property('bChunked')
        report.append(dict(actor=gate.get_actor_label(),chunks=len(gate.chunks),triangles=gate.triangle_count,max_chunk=gate.largest_chunk_triangles,budget=gate.max_triangles))
    for group in item['clod_meshes']:
        a=next(a for a in rocks if a.get_actor_label()==f'MAIN_GAME_ClayWallClods_{i+1}_{group["guide"]}')
        assert unreal.MiningDesignerLibrary.import_campaign_rock(a,str(ART/group['file']))
assert levels.save_current_level()
(ROOT/'Saved/Validation/MainGameRoutesV5/ClayRelief/chunks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.SystemLibrary.execute_console_command(world,'QUIT_EDITOR')
