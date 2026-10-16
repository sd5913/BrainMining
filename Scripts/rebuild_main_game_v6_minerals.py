"""Sparse main routes and one guaranteed top-tier reward per terminal branch."""
import unreal,json,sys,math
from pathlib import Path
from collections import Counter
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
from main_game_material_plan import mineral_ids
STAGES=['TunnelV1','TunnelCavernV2','TunnelStalagmiteV3']
def rebuild(hall,actors):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();field=api.get_field(hall);recipe=json.loads((ROOT/'Config/MainGameMinerals.json').read_text())
    definitions=list(field.definitions)
    iron=next(d for d in definitions if str(d.id)=='4')
    iron.set_editor_property('color',unreal.LinearColor(.60,.045,.028,1));iron.set_editor_property('hint_color',unreal.LinearColor(.82,.08,.035,1))
    iron.set_editor_property('roughness',.58);iron.set_editor_property('metallic',.18);iron.set_editor_property('emissive_strength',.08)
    by_id={str(d.id):d for d in definitions};selected=[];rewards=[];distribution=[]
    all_actors=actors.get_all_level_actors();candidates=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
    for i,stage in enumerate(STAGES):
        tag=f'BM_Campaign_MainGame{i+1:02}';highest=[4,6,9][i]
        region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(tag))
        origin=region.get_actor_location()+region.cutters[0].local_transform.translation
        paths=json.loads((ROOT/'Art/MainGame/RoutesV6'/stage/'centerlines.json').read_text())
        raw=lambda p:origin+unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
        ignored=[a for a in all_actors if isinstance(a,unreal.MiningTreasure)]
        for branch in paths[1:]:
            p=raw(branch['points'][-5]);t=(raw(branch['points'][-1])-p).normal();side=t.cross(unreal.Vector(0,0,1)).normal()
            options=[]
            for d in [t,(t+side*.3).normal(),(t-side*.3).normal(),side,-side,unreal.Vector(0,0,1)]:
                h=unreal.SystemLibrary.line_trace_single(world,p,p+d*1500,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignored,unreal.DrawDebugTrace.NONE,True)
                h=h.to_tuple() if h else None
                if not h or not h[0] or h[9]!=region.generated_rock:continue
                if h[5].distance(raw(branch['points'][-1]))>650:continue
                options.append(h)
            assert options,('No terminal wall',stage,branch['name'])
            h=options[0]
            placement=unreal.MiningTreasurePlacement(area_tag=tag,route_progress=branch['root_arc']/paths[0]['arc'][-1],guide_layer=False,treasure_id=str(highest),value=by_id[str(highest)].values_by_size[1],surface=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7])),transform=unreal.Transform(location=h[5]-h[7]*120),hosts=[region.generated_rock])
            rewards.append(dict(region=i+1,branch=branch['name'],mineral=highest,index=len(selected),terminal=list(raw(branch['points'][-1]).to_tuple()),surface=list(h[5].to_tuple())))
            selected.append(placement)
        count=[26,30,33][i];ids=mineral_ids(recipe['areas'][i],count,2026101606)
        choices=[p for p in candidates if str(p.area_tag)==tag]
        for j,mineral in enumerate(ids):
            progress=(j+.5)/count
            ranked=sorted(choices,key=lambda p:abs(p.route_progress-progress)+(.1 if bool(p.guide_layer)!=(j%6==0) else 0))
            chosen=next((p for p in ranked if all(p.surface.translation.distance(q.surface.translation)>230 for q in selected)),None)
            assert chosen,('Main route density exhausted candidates',i,j)
            choices.remove(chosen);chosen.set_editor_property('treasure_id',str(mineral));chosen.set_editor_property('value',by_id[str(mineral)].values_by_size[j%3]);selected.append(chosen)
        distribution.append(dict(region=i+1,main=count,dead_ends=4,minerals=dict(Counter(str(p.treasure_id) for p in selected if str(p.area_tag)==tag))))
        unreal.log('V6 mineral selection region '+str(i+1)+' complete')
    staged=api._stage(hall,actors,field)
    try:
        staged.set_editor_property('definitions',definitions);staged.set_editor_property('authored_placements',selected)
        assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(staged),'Mineral burial failed'
        validation=api.validate(staged,hall);assert validation['ok'],validation.get('errors')
        layout=list(staged.get_editor_property('authored_placements'))
        for reward in rewards:
            p=layout[reward['index']];assert str(p.treasure_id)==str(reward['mineral'])
            assert p.surface.translation.distance(unreal.Vector(*reward['terminal']))<650
            reward['core']=list(p.transform.translation.to_tuple())
        api._commit(staged,field,actors)
    except Exception:
        actors.destroy_actor(staged);raise
    return dict(validation=validation,rewards=rewards,distribution=distribution,total=len(selected))
