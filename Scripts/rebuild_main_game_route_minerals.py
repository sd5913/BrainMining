"""MainGame-only mineral siting: 80% normal rock, about 30% cave ceilings.

Reuses the existing economy plan, burial/clue validation and transactional commit.
The 125 rewards and upgrade budgets remain unchanged.
"""
import unreal,json,sys,math,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
from campaign_mineral_plan import make_plan
OUT=ROOT/'Saved/Validation/MainGameRoutesV2';report={'passed':False}
def xyz(v):return list(v.to_tuple())
try:
    world,hall,actors=api._context();field=api.get_field(hall)
    assert field and world.get_path_name()=='/Game/MainGame/Maps/Lvl_MainGame.Lvl_MainGame'
    recipe=json.loads((ROOT/'Config/MainGameMinerals.json').read_text());plan=make_plan(recipe)
    candidates=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
    all_actors=actors.get_all_level_actors();regions=[a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_RoutesV2')]
    ignore=[g for r in regions for g in r.generated_guide_layers]
    for r in regions:
        tag=next(str(t) for t in r.tags if str(t).startswith('BM_Campaign_'))
        area=next(a for a in hall.areas if str(a.tag)==tag);route=list(area.map_route)
        arcs=[0.]
        for a,b in zip(route,route[1:]):arcs.append(arcs[-1]+a.distance(b))
        for i in range(3,len(route)-3,4):
            p=route[i]
            for direction in [unreal.Vector(0,0,1),unreal.Vector(.22,0,.975),unreal.Vector(-.22,0,.975),unreal.Vector(0,.22,.975),unreal.Vector(0,-.22,.975)]:
                h=unreal.SystemLibrary.line_trace_single(world,p,p+direction*1900,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True)
                h=h.to_tuple() if h else None
                if not h or not h[0] or h[9]!=r.generated_rock or not (-.9998<h[7].z<-.50):continue
                s=unreal.MiningTreasurePlacement(area_tag=tag,route_progress=arcs[i]/arcs[-1],guide_layer=False,
                    surface=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7])),
                    transform=unreal.Transform(location=h[5]-h[7]*120),hosts=[r.generated_rock])
                candidates.append(s)
    def ceiling(p):return p.surface.rotation.rotator().pitch < -30
    selected=[];summaries=[]
    for phase in plan:
        band=phase['band'];lo,hi=band['range'];tag='BM_Campaign_'+band['area']
        sites=[p for p in candidates if str(p.area_tag)==tag and lo<=p.route_progress<=hi]
        for index,entry in enumerate(phase['entries']):
            mode=index%10;want_ceiling=mode<3;want_guide=mode>=8
            available=[p for p in sites if bool(p.guide_layer)==want_guide and ceiling(p)==want_ceiling and all(p.transform.translation.distance(q.transform.translation)>=band['spacing'] for q in selected)]
            assert available,('Insufficient sites',tag,band['phase'],index,want_ceiling,want_guide)
            progress=lo+(hi-lo)*(index+.5)/len(phase['entries'])
            p=min(available,key=lambda p:(abs(p.route_progress-progress),*xyz(p.transform.translation)))
            sites.remove(p);p.set_editor_property('treasure_id',str(entry['id']));p.set_editor_property('value',entry['value']);selected.append(p)
        summaries.append(dict(area=band['area'],phase=band['phase'],count=len(phase['entries']),total=phase['total'],budget=band['budget']))
    assert len(selected)==125
    stage=api._stage(hall,actors,field)
    try:
        stage.set_editor_property('authored_placements',selected)
        assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage),'Native mineral burial/clue preparation failed'
        validation=api.validate(stage,hall);assert validation['ok'],validation.get('errors')
        layout=list(stage.get_editor_property('authored_placements'))
        distribution=[]
        for r in regions:
            tag=next(str(t) for t in r.tags if str(t).startswith('BM_Campaign_'));group=[p for p in layout if str(p.area_tag)==tag]
            count=sum(not p.guide_layer for p in group);tops=sum(ceiling(p) for p in group)
            assert count/len(group)>=.75 and tops>=4,(tag,count,tops)
            distribution.append(dict(area=tag,total=len(group),normal=count,guide=len(group)-count,ceiling=tops))
        api._commit(stage,field,actors)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        report.update(passed=True,saved=True,deposits=125,distribution=distribution,summary=summaries,layout_sha256=validation['layout_sha256'],fragments=validation['fragments'])
    except Exception:
        actors.destroy_actor(stage);raise
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'minerals.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('ROUTES_MINERALS '+json.dumps(report,ensure_ascii=False))
