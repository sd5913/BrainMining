"""One-time, backed-up migration of junction geometry and deeper mineral veins."""
import unreal, runpy, json, shutil, hashlib
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir())
recipe=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))
backup=root/'Saved/CampaignBackups'/('junctions-v2-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
for rel in ['Campaign/Maps/Lvl_FullJourney.umap','Campaign/Materials/M_MinerHeadlamp.uasset','Showcase/Treasure/Meshes/SM_CrystalClue.uasset','Showcase/Treasure/Materials/M_TreasureMineral.uasset']:
    shutil.copy2(root/'Content'/rel,backup/Path(rel).name)
hashes={s['source']:hashlib.sha256((root/'Content'/(s['source'][6:]+'.umap')).read_bytes()).hexdigest() for s in recipe['stages']}
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(recipe['map'])
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
hall=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
field=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements'))
already=field.actor_has_tag('BM_DeeperCrystalVeinsV2')
before=[dict(depth=(p.transform.translation-p.surface.translation).length(),fragments=len(p.fragments),core=sum((q.translation-p.transform.translation).length()<160 for q in p.fragments)) for p in field.get_editor_property('authored_placements')]
runpy.run_path(str(root/'Scripts/campaign_materials.py'))['ensure_miner_headlamp']()
material=unreal.load_asset('/Game/Showcase/Treasure/Materials/M_TreasureMineral')
lib=unreal.MaterialEditingLibrary
base=next(e for e in lib.get_material_expressions(material) if isinstance(e,unreal.MaterialExpressionCustom) and 'float3 color = Tint.rgb' in e.get_editor_property('code'))
code=base.get_editor_property('code')
if 'Style < -.5' not in code:
    code=code.replace('float3 color = Tint.rgb', 'if (Style < -.5) return Tint.rgb * lerp(.35, 1.05, saturate((V.r-.72)/.28));\nfloat3 color = Tint.rgb')
    base.set_editor_property('code',code)
lib.recompile_material(material);assert unreal.EditorAssetLibrary.save_loaded_asset(material)
assert unreal.MiningDesignerLibrary.build_crystal_clue_mesh()
assert unreal.MiningDesignerLibrary.naturalize_campaign_junctions(hall)
placements=list(field.get_editor_property('authored_placements'))
for p in placements:
    p.set_editor_property('surface',unreal.Transform(location=p.surface.translation,rotation=unreal.MathLibrary.find_look_at_rotation(p.transform.translation,p.surface.translation)))
field.set_editor_property('authored_placements',placements)

if not field.actor_has_tag('BM_DeeperCrystalVeinsV2'):
    assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(field)
    placements=list(field.get_editor_property('authored_placements'))
    for index,(p,previous) in enumerate(zip(placements,before)):
        if (p.transform.translation-p.surface.translation).length()>previous['depth']+20:continue
        field.set_editor_property('authored_placements',[p])
        assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(field)
        p=field.get_editor_property('authored_placements')[0]
        keep=[];supports=[];core_count=0
        for chip,support in zip(p.fragments,p.fragment_supports):
            if (chip.translation-p.transform.translation).length()<160:
                if core_count>=2*previous['core']:continue
                core_count+=1
            keep.append(chip);supports.append(support)
        p.set_editor_property('fragments',keep);p.set_editor_property('fragment_supports',supports)
        placements[index]=p
    for p in placements:
        p.set_editor_property('surface',unreal.Transform(location=p.surface.translation,rotation=unreal.MathLibrary.find_look_at_rotation(p.transform.translation,p.surface.translation)))
    field.set_editor_property('authored_placements',placements)
    field.set_editor_property('tags',list(field.tags)+['BM_DeeperCrystalVeinsV2'])
rows=[]
for p,old in zip(field.get_editor_property('authored_placements'),before):
    new=dict(depth=(p.transform.translation-p.surface.translation).length(),fragments=len(p.fragments),core=sum((q.translation-p.transform.translation).length()<160 for q in p.fragments))
    rows.append(dict(id=str(p.treasure_id),old=old,new=new))
    assert already or new['depth']>old['depth'],(old,new)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world,recipe['map'])
assert all(hashlib.sha256((root/'Content'/(p[6:]+'.umap')).read_bytes()).hexdigest()==h for p,h in hashes.items())
if not already: (root/'Saved/CampaignJunctionsV2.json').write_text(json.dumps(dict(backup=str(backup),minerals=rows,source_maps_unchanged=True),indent=2),encoding='utf-8')
assert unreal.MiningDesignerLibrary.export_campaign_layout(hall,str(root/'Art/Mining/FullJourneyExchange/Source'))
unreal.log('CAMPAIGN_JUNCTIONS_V2_SAVED')
