"""Retain the existing reward plan; add ten low-tier wall deposits per area.

Ceiling deposits are retained in the base plan. Extra deposits are explicitly
ordinary side-wall sites, not guide plugs or ceilings. Prices remain unchanged.
"""
import unreal,json,sys,math,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
from campaign_mineral_plan import make_plan
OUT=ROOT/'Saved/Validation/MainGamePolish';report={'passed':False}
cfg=json.loads((ROOT/'Config/MainGameTuning.json').read_text());bonus=cfg['bonus_wall_deposits_per_area']
def xyz(v):return tuple(v.to_tuple())
try:
    assert json.loads((OUT/'authoring.json').read_text())['passed']
    world,hall,actors=api._context();field=api.get_field(hall)
    recipe=json.loads((ROOT/'Config/MainGameMinerals.json').read_text());plan=make_plan(recipe)
    candidates=list(unreal.MiningDesignerLibrary.find_campaign_mineral_sites(hall))
    regions=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag('BM_MainGame_RoutesV2')]
    ignore=[g for r in regions for g in r.generated_guide_layers]
    for r in regions:
        tag=next(str(t) for t in r.tags if str(t).startswith('BM_Campaign_'));area=next(a for a in hall.areas if str(a.tag)==tag);route=list(area.map_route);arcs=[0.]
        for a,b in zip(route,route[1:]):arcs.append(arcs[-1]+a.distance(b))
        for i in range(3,len(route)-3,4):
            p=route[i]
            for direction in [unreal.Vector(0,0,1),unreal.Vector(.22,0,.975),unreal.Vector(-.22,0,.975),unreal.Vector(0,.22,.975),unreal.Vector(0,-.22,.975)]:
                h=unreal.SystemLibrary.line_trace_single(world,p,p+direction*1900,unreal.TraceTypeQuery.ECC_VISIBILITY,True,ignore,unreal.DrawDebugTrace.NONE,True);h=h.to_tuple() if h else None
                if not h or not h[0] or h[9]!=r.generated_rock or not (-.9998<h[7].z<-.50):continue
                candidates.append(unreal.MiningTreasurePlacement(area_tag=tag,route_progress=arcs[i]/arcs[-1],guide_layer=False,surface=unreal.Transform(location=h[5],rotation=unreal.MathLibrary.make_rot_from_x(h[7])),transform=unreal.Transform(location=h[5]-h[7]*120),hosts=[r.generated_rock]))
    # Cache native properties once: selection and distance tests then run on
    # ordinary Python values, rather than millions of UObject property calls.
    cache=[dict(p=p,tag=str(p.area_tag),progress=p.route_progress,guide=bool(p.guide_layer),pitch=p.surface.rotation.rotator().pitch,pos=xyz(p.transform.translation)) for p in candidates]
    positions=[];selected=[];used=set();summary=[]
    def choose(tag,lo,hi,progress,guide,top,spacing):
        options=(r for r in cache if id(r) not in used and r['tag']==tag and lo<=r['progress']<=hi and r['guide']==guide and (r['pitch'] < -30)==top and all(math.dist(r['pos'],q)>=spacing for q in positions))
        best=min(options,key=lambda r:(abs(r['progress']-progress),*r['pos']),default=None)
        assert best,('Insufficient mineral sites',tag,progress,guide,top)
        used.add(id(best));positions.append(best['pos']);return best['p']
    for phase in plan:
        b=phase['band'];tag='BM_Campaign_'+b['area'];lo,hi=b['range']
        for i,e in enumerate(phase['entries']):
            mode=i%10;p=choose(tag,lo,hi,lo+(hi-lo)*(i+.5)/len(phase['entries']),mode>=8,mode<3,b['spacing'])
            p.set_editor_property('treasure_id',str(e['id']));p.set_editor_property('value',e['value']);selected.append(p)
        summary.append(dict(area=b['area'],phase=b['phase'],base_count=len(phase['entries']),base_value=phase['total']))
    assert len(selected)==125
    bonuses=[]
    for n in range(1,5):
        name='MainGame%02d'%n;tag='BM_Campaign_'+name;bands=[b for b in recipe['bands'] if b['area']==name]
        mineral=min(i for b in bands for i in b['ids']);value=recipe['values'][mineral-1][0]
        for i in range(bonus):
            p=choose(tag,.035,.94,.07+.85*(i+.5)/bonus,False,False,max(b['spacing'] for b in bands))
            p.set_editor_property('treasure_id',str(mineral));p.set_editor_property('value',value);selected.append(p)
        bonuses.append(dict(area=name,count=bonus,mineral=mineral,value_each=value,total=bonus*value))
    assert len(selected)==cfg['expected_deposits']
    stage=api._stage(hall,actors,field)
    try:
        stage.set_editor_property('authored_placements',selected)
        assert unreal.MiningDesignerLibrary.prepare_campaign_minerals(stage),'Mineral burial/clue preparation failed'
        valid=api.validate(stage,hall);assert valid['ok'],valid.get('errors')
        layout=list(stage.get_editor_property('authored_placements'));distribution=[]
        for n in range(1,5):
            tag='BM_Campaign_MainGame%02d'%n;group=[p for p in layout if str(p.area_tag)==tag]
            distribution.append(dict(area=tag,total=len(group),normal=sum(not p.guide_layer for p in group),guide=sum(p.guide_layer for p in group),ceiling=sum(p.surface.rotation.rotator().pitch<-30 for p in group)))
        assert sum(r['normal'] for r in distribution)>=145
        api._commit(stage,field,actors);assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        report.update(passed=True,saved=True,deposits=len(layout),distribution=distribution,bonus_wall_deposits=bonuses,base_plan=summary,base_rewards_preserved=True,upgrade_prices_unchanged=True,fragments=valid['fragments'],layout_sha256=valid['layout_sha256'])
    except Exception:actors.destroy_actor(stage);raise
except Exception:report['error']=traceback.format_exc();raise
finally:
    (OUT/'minerals.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('POLISH_MINERALS '+json.dumps(report,ensure_ascii=False))
