"""Bake smaller junction chunks and compact pre-generated crystal veins."""
import json
import runpy
import shutil
from datetime import datetime
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
backup=root/'Saved/CampaignBackups'/('mining-stability-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
for rel in ['Campaign/Maps/Lvl_FullJourney.umap','Campaign/Materials/M_MinerHeadlamp.uasset']:
    shutil.copy2(root/'Content'/rel,backup/Path(rel).name)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
hall=next(a for a in actors if isinstance(a,unreal.MiningExhibitHall))
field=next(a for a in actors if isinstance(a,unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements'))
before=[(p.transform.translation,p.value,len(p.fragments)) for p in field.get_editor_property('authored_placements')]
assert unreal.MiningDesignerLibrary.optimize_campaign_terrain(hall)
assert unreal.MiningDesignerLibrary.refine_campaign_crystal_clues(field)
after=field.get_editor_property('authored_placements')
assert len(after)==len(before)
for p,(core,value,_) in zip(after,before):
    assert (p.transform.translation-core).length()<.001 and p.value==value
runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_miner_headlamp']()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/Campaign/Maps/Lvl_FullJourney')
report=dict(backup=str(backup),deposits=len(after),before_fragments=sum(b[2] for b in before),
            after_fragments=sum(len(p.fragments) for p in after),counts=[len(p.fragments) for p in after],
            chunked_junctions=[dict(name=a.get_actor_label(),chunks=len(a.get_editor_property('chunks')),
                                   triangles=a.triangle_count,largest=a.largest_chunk_triangles)
                for a in actors if isinstance(a,unreal.GeneratedMiningRock) and a.get_actor_label().startswith('Campaign_') and a.get_editor_property('chunked')])
(root/'Saved/MiningStabilityBake.json').write_text(json.dumps(report,indent=2))
unreal.log('MINING_STABILITY_BAKED '+json.dumps(report))
