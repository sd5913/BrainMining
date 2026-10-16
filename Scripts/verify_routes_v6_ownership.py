"""Read-only source-volume ownership and saved dead-end reward verification."""
import unreal,json,sys,traceback,random
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Scripts'))
import campaign_minerals_api as api
OUT=ROOT/'Saved/Validation/MainGameRoutesV6';report={'passed':False,'pairs':[]}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);lib=unreal.MiningDesignerLibrary
spatial=unreal.GeometryScript_MeshSpatial;options=unreal.GeometryScriptSpatialQueryOptions()
def source(a):
    mesh=lib.get_campaign_rock_source(a);assert mesh
    bvh=spatial.build_bvh_for_mesh(mesh)[1]
    return mesh,bvh,a.get_actor_transform()
def inside(s,p):
    return spatial.is_point_inside_mesh(s[0],s[1],unreal.MathLibrary.inverse_transform_location(s[2],p),options)[1]
try:
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world,hall,actors=api._context();all_actors=actors.get_all_level_actors()
    assert hall.actor_has_tag('BM_RoutesV6')
    rng=random.Random(1926)
    for i in range(3):
        region=next(a for a in all_actors if isinstance(a,unreal.MiningExternalRegion) and a.actor_has_tag(f'BM_Campaign_MainGame{i+1:02}'))
        wall=source(region.generated_rock)
        for guide in region.generated_guide_layers:
            owned=source(guide);positions=unreal.GeometryScript_List.convert_vector_list_to_array(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(owned[0],True)[1])
            low=[min(p.to_tuple()[j] for p in positions) for j in range(3)];high=[max(p.to_tuple()[j] for p in positions) for j in range(3)];count=0;overlap=0
            for k in range(12000):
                p=unreal.MathLibrary.transform_location(owned[2],unreal.Vector(*[rng.uniform(low[j],high[j]) for j in range(3)]))
                if not inside(owned,p):continue
                count+=1;overlap+=int(inside(wall,p))
                if count>=100:break
            assert count>10 and overlap==0,(guide.get_actor_label(),count,overlap)
            report['pairs'].append(dict(region=i+1,layer=guide.get_actor_label(),inside_samples=count,overlapping_samples=overlap))
    field=api.get_field(hall);valid=api.validate(field,hall);assert valid['ok'];layout=list(field.get_editor_property('authored_placements'));assert len(layout)==101
    iron=next(d for d in field.definitions if str(d.id)=='4');diamond=next(d for d in field.definitions if str(d.id)=='9')
    assert max(iron.color.r,iron.color.g,iron.color.b)<.05 and iron.emissive_strength<=.03
    report['iron_color']=list(iron.color.to_tuple());report['diamond_color']=list(diamond.color.to_tuple())
    author=json.loads((OUT/'authoring.json').read_text())
    for reward in author['minerals']['rewards']:
        p=layout[reward['index']];assert str(p.treasure_id)==str(reward['mineral']) and p.surface.translation.distance(unreal.Vector(*reward['terminal']))<650
    report.update(passed=True,minerals=valid,dead_end_rewards=12)
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (OUT/'ownership.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.SystemLibrary.execute_console_command(ed.get_editor_world(),'QUIT_EDITOR')
