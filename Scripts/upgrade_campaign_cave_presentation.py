"""Backed-up in-place cave/material update; room source maps remain linked."""
import unreal,runpy,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir());recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
backup=root/'Saved/CampaignBackups'/('cave-presentation-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
for relative in ['Content/Campaign/Maps/Lvl_FullJourney.umap','Content/Campaign/Materials/M_CampaignLimestone.uasset','Content/Campaign/Materials/M_CampaignStainedEarth.uasset']:
    p=root/relative
    if p.exists():shutil.copy2(p,backup/p.name)
hashes={s['source']:hashlib.sha256((root/'Content'/(s['source'][6:]+'.umap')).read_bytes()).hexdigest() for s in recipe['stages']}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(recipe['map']);world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mat=runpy.run_path(str(root/'Scripts/campaign_materials.py'));dark=mat['ensure_campaign_limestone']();guide=mat['ensure_campaign_limestone'](True);stain=mat['ensure_campaign_stain']();mat['ensure_miner_headlamp']()
hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall));changes=[]
for a in actors.get_all_level_actors():
    if a.get_level()!=hall.get_level():continue
    if isinstance(a,unreal.MiningExternalRegion):a.set_editor_property('rock_material',dark)
    if not isinstance(a,unreal.GeneratedMiningRock):continue
    name=a.get_actor_label();old=a.get_editor_property('rock_material');path=old.get_path_name() if old else ''
    # Imported guide blockers are separate actors, with their deliberately brown minerals.
    a.set_editor_property('rock_material',stain if 'StainedEarth' in path else guide if '引导' in name else dark);a.reset_rock()
    if name.startswith('Campaign_Tunnel') and not a.actor_has_tag('BM_SoftPassageV1'):
        assert unreal.MiningDesignerLibrary.soften_campaign_passage(a),name
        a.set_editor_property('tags',list(a.tags)+['BM_SoftPassageV1'])
    changes.append(dict(actor=name,old_material=path,triangles=a.triangle_count))
assert unreal.EditorLoadingAndSavingUtils.save_map(world,recipe['map'])
assert all(hashlib.sha256((root/'Content'/(p[6:]+'.umap')).read_bytes()).hexdigest()==h for p,h in hashes.items())
(root/'Saved/CampaignCavePresentation.json').write_text(json.dumps(dict(backup=str(backup),changes=changes,source_maps_unchanged=True),ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('CAMPAIGN_CAVE_PRESENTATION_SAVED')
