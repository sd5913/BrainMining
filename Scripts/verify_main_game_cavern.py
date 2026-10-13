"""Reload-only checks of real cavern columns, guides, shell and connection seams."""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelCavernV2'
report={'passed':False,'regenerated':False};started=time.monotonic()
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(w,a,b,ignore=None):return unreal.SystemLibrary.line_trace_single(w,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore or [],unreal.DrawDebugTrace.NONE,True)
def actor(h):return h.to_tuple()[9] if h and h.to_tuple()[0] else None
def source(mesh):
    geo=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,geo,unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0));return geo
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    regions=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion)
    find=lambda tag:next(r for r in regions if tag in [str(t) for t in r.tags])
    old=find('BM_MainGame_V1');new=find('BM_MainGame_CavernV2');reserve=find('BM_MainGame_FutureRoom')
    assert len(old.generated_guide_layers)==16 and len(new.generated_guide_layers)==16 and len(reserve.generated_guide_layers)==0
    origin=new.get_actor_location()+new.cutters[0].local_transform.translation
    guides=list(new.generated_guide_layers);checks=[]
    for guide,record in zip(guides,json.loads((ART/'placement_plan.json').read_text())['objects']):
        p=raw(record['center_m'])+origin;span=(record['arc_length_m']+15)*100
        ignore=[r.generated_rock for r in regions]+[r.generated_shell for r in regions]+[g for r in regions for g in r.generated_guide_layers if g!=guide]
        assert actor(trace(world,p+unreal.Vector(0,0,span),p-unreal.Vector(0,0,span),ignore))==guide,guide.get_name()
        before=guide.triangle_count;assert guide.mine_sphere(p,120),str(guide.last_status)
        checks.append(dict(name=guide.get_name(),collision=True,excavated=True,triangles=before))
    new.reset_guide_excavation();assert all(g.successful_cuts==0 and g.triangle_count==r['triangles'] for g,r in zip(guides,checks))
    ignore=[g for r in regions for g in r.generated_guide_layers]
    pillars=[]
    for spec in json.loads((ART/'pillars.json').read_text()):
        p=raw(spec['center_m'])+origin
        assert actor(trace(world,p+unreal.Vector(220,0,0),p,ignore))==new.generated_rock
        assert actor(trace(world,p-unreal.Vector(220,0,0),p,ignore))==new.generated_rock
        pillars.append(dict(name=spec['name'],collision=True))
    for r in [new,reserve]:
        c=r.get_actor_location();outside=c+unreal.Vector(0,r.rock_size.y*.5+r.shell_thickness+150,0);inside=c+unreal.Vector(0,r.rock_size.y*.5+40,0)
        assert actor(trace(world,outside,inside))==r.generated_shell
    old_origin=old.get_actor_location()+old.cutters[0].local_transform.translation
    for x in [6250,8550]:
        z=900-(x-6500)*.10
        for y in [-60,0,60]:assert actor(trace(world,old_origin+unreal.Vector(x-150,y,z),old_origin+unreal.Vector(x+150,y,z),ignore)) is None
    vol=lambda geo:abs(unreal.GeometryScript_MeshQueries.get_mesh_volume_area(geo)[1])
    blocked=sum(vol(source(g.mesh)) for g in new.guide_layers);total=vol(source(new.cutters[0].mesh))
    assert .45<=blocked/total<=.55
    for g in new.guide_layers:
        geo=source(g.mesh);assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(geo)
        _,tree=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(geo)
        for path in json.loads((ART/'centerlines.json').read_text()):
            _,inside,_=unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(geo,tree,raw(path['points'][-1]),unreal.GeometryScriptSpatialQueryOptions());assert not inside
    report.update(passed=True,region_count=len(regions),previous_guides=16,new_guides=checks,columns=pillars,
        shell_collision=True,seams_open=True,reserve_has_no_guides=True,all_terminals_clear=True,blocker_fill_fraction=blocked/total,
        new_rock_chunks=len(new.generated_rock.chunks),reserve_rock_chunks=len(reserve.generated_rock.chunks))
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
report['elapsed_seconds']=time.monotonic()-started
(ROOT/'Saved/Validation/cavern_reload.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('CAVERN_RELOAD '+json.dumps(report))
