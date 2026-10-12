"""Patch only authored mineral placements; preserve the existing excavatable terrain.

Called by create_continuous_campaign.py, or directly with -ExecutePythonScript.
"""
import json, math, runpy, shutil
from collections import Counter
from datetime import datetime
from pathlib import Path
import unreal

def populate(world, hall):
    root=Path(unreal.Paths.project_dir())
    recipe=json.loads((root/'Config/CampaignMinerals.json').read_text(encoding='utf-8'))
    plan=runpy.run_path(str(root/'Scripts/campaign_mineral_plan.py'))['make_plan'](recipe)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,unreal.MiningTreasureField) and actor.get_editor_property('use_authored_placements'):
            actors.destroy_actor(actor)
    field=actors.spawn_actor_from_class(unreal.MiningTreasureField,unreal.Vector(0,-110000,0))
    field.set_actor_label('CAMPAIGN_Minerals_EditConfigToRebalance')
    field.set_editor_property('use_shared_wallet',True)
    field.set_editor_property('use_authored_placements',True)
    field.set_editor_property('random_seed',recipe['seed'])
    mineral_material=field.treasure_material
    mineral_material.set_editor_property('used_with_instanced_static_meshes',True)
    unreal.MaterialEditingLibrary.recompile_material(mineral_material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mineral_material)
    definitions=list(field.definitions)
    for i,d in enumerate(definitions):
        d.set_editor_property('value_tiers',recipe['values'][i])
        d.set_editor_property('min_value',min(recipe['values'][i]))
        d.set_editor_property('max_value',max(recipe['values'][i]))
        d.set_editor_property('scale_with_value',True)
        d.set_editor_property('regions',['BM_Campaign_Mine%02d'%(i//3+1)])
        d.set_editor_property('scale',unreal.Vector(.7,.7,.7))
    field.set_editor_property('definitions',definitions)
    candidates=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
    selected=[];summary=[]
    for phase in plan:
        band=phase['band'];tag='BM_Campaign_'+band['area'];lo,hi=band['range']
        sites=[p for p in candidates if str(p.area_tag)==tag and lo<=p.route_progress<=hi]
        assert sites, 'No mineral surfaces for '+str(band)
        for j,entry in enumerate(phase['entries']):
            progress=lo+(hi-lo)*(j+.5)/len(phase['entries'])
            guide=j%5<2
            def clear(p):
                v=p.transform.translation
                return all(math.dist((v.x,v.y,v.z),(q.transform.translation.x,q.transform.translation.y,q.transform.translation.z))>=band['spacing'] for q in selected)
            available=[p for p in sites if clear(p)]
            assert available, 'Insufficient spaced mineral sites: '+str(band)
            p=min(available,key=lambda s:abs(s.route_progress-progress)+(0 if s.guide_layer==guide else .065))
            sites.remove(p)
            p.set_editor_property('treasure_id',str(entry['id']))
            p.set_editor_property('value',entry['value'])
            selected.append(p)
        summary.append(dict(area=band['area'],phase=band['phase'],count=len(phase['entries']),total=phase['total'],upgrade_cost=band['upgrade_cost'],type_weights=band['weights'],counts=dict(Counter(e['id'] for e in phase['entries']))))
    field.set_editor_property('authored_placements',selected)
    assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(field), 'Mineral burial/clue validation failed'
    rows=[]
    for p in field.get_editor_property('authored_placements'):
        rows.append(dict(area=str(p.area_tag),id=str(p.treasure_id),value=p.value,progress=p.route_progress,guide=p.guide_layer,position=[p.transform.translation.x,p.transform.translation.y,p.transform.translation.z],fragments=len(p.fragments),hosts=[h.get_actor_label() for h in p.hosts]))
    (root/'Saved/CampaignMineralsManifest.json').write_text(json.dumps(dict(summary=summary,placements=rows,recipe=recipe),ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('CAMPAIGN_MINERALS_READY '+str(len(rows)))
    return field

if __name__=='__main__':
    root=Path(unreal.Paths.project_dir())
    target=json.loads((root/'Config/CampaignLayout.json').read_text(encoding='utf-8'))['map']
    backup=root/'Saved/CampaignBackups'/('minerals-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    shutil.copy2(root/'Content'/(target[6:]+'.umap'),backup/'Previous.umap')
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    hall=next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.MiningExhibitHall))
    populate(world,hall)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,target)
    unreal.log('CAMPAIGN_MINERALS_SAVED '+target)
    unreal.SystemLibrary.quit_editor()
