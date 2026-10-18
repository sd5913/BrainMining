"""Read-only surface and source-room inspection for collectible placement."""
import json,unreal,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/Validation/MainGameCollections';OUT.mkdir(parents=True,exist_ok=True)
cfg=json.loads((ROOT/'Config/MainGameCollections.json').read_text())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def xyz(p):return list(p.to_tuple())
def trace(p,q):
    h=unreal.SystemLibrary.line_trace_single(ed.get_editor_world(),p,q,unreal.TraceTypeQuery.ECC_VISIBILITY,True,[],unreal.DrawDebugTrace.NONE,True)
    return h.to_tuple() if h else None
report={}
try:
    assert levels.load_level(cfg['source_map'])
    aa=list(api.get_all_level_actors());hall=next(a for a in aa if isinstance(a,unreal.MiningExhibitHall));report['sites']=[]
    for spec in cfg['mines']:
        area=next(a for a in hall.areas if str(a.tag)==spec['area_tag']);route=list(area.map_route);k=round((len(route)-1)*spec['route_fraction']);candidates=[]
        for j in [k,k-3,k+3,k-6,k+6]:
            p=route[j];f=trace(p,p-unreal.Vector(0,0,3000))
            if not f:continue
            ground=f[5];start=unreal.Vector(p.x,p.y,ground.z+spec.get('height_cm',160));direction=unreal.Vector(0,spec['side'],0)
            if spec.get('floor'):
                start=p+direction*90;h=trace(start,start-unreal.Vector(0,0,3000))
            else:h=trace(start,start+direction*2300)
            if not h:continue
            candidates.append(dict(index=j,route=xyz(p),ground=xyz(ground),surface=xyz(h[5]),normal=xyz(h[7]),host=h[9].get_name() if h[9] else None,host_type=h[9].get_class().get_name() if h[9] else None))
        report['sites'].append(dict(spec=spec,candidates=candidates))
    report['office']=[dict(label=a.get_actor_label(),name=a.get_name(),position=xyz(a.get_actor_location()),bounds=[xyz(v) for v in a.get_actor_bounds(False)],materials=[str(m) for c in a.get_components_by_class(unreal.MeshComponent) for m in c.get_materials()]) for a in aa if 'PopupClose1' in a.get_level().get_path_name() and a.get_actor_label() in ['SM_Plane','SM_KitchenChair_01a','SM_Sofa_01b','SM_Sofa_02a']]
    assert levels.load_level('/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet')
    report['heaven']=[]
    for a in api.get_all_level_actors():
        row=dict(label=a.get_actor_label(),type=a.get_class().get_name(),position=xyz(a.get_actor_location()),rotation=str(a.get_actor_rotation()))
        if isinstance(a,unreal.MiningCollectionField):row['placements']=[dict(id=p.item_id,position=xyz(p.transform.translation),presentation=str(p.presentation_actor)) for p in a.authored_placements]
        report['heaven'].append(row)
    (OUT/'probe.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
except Exception:
    (OUT/'probe_error.txt').write_text(traceback.format_exc());raise
finally:unreal.SystemLibrary.quit_editor()
