"""Fresh-process saved-map verification; no regeneration and no map save."""
import unreal,json,traceback,time
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ART=ROOT/'Art/MainGame/TunnelV1'
report={'passed':False,'regenerated':False};started=time.monotonic()
def raw(p):return unreal.Vector(p[0]*100,-p[1]*100,p[2]*100)
def trace(world,a,b,ignore=None):return unreal.SystemLibrary.line_trace_single(world,a,b,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore or [],unreal.DrawDebugTrace.NONE,True)
def hit_actor(hit):return hit.to_tuple()[9] if hit else None
def source(mesh):
    geo=unreal.DynamicMesh();unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,geo,
        unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0));return geo
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    regions=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.MiningExternalRegion);assert len(regions)==1
    region=regions[0];rock,shell=region.generated_rock,region.generated_shell;guides=list(region.generated_guide_layers)
    assert len(guides)==16 and not isinstance(shell,unreal.DiggableRock)
    placements=json.loads((ART/'placement_plan.json').read_text())['objects'];checks=[]
    for i,(guide,record) in enumerate(zip(guides,placements)):
        assert guide.get_owner()==region
        center=raw(record['center_m']);distance=(record['arc_length_m']+4)*100
        ignore=[rock,shell]+[other for other in guides if other!=guide]
        # A vertical probe passes through each solid, independently of nearby cave walls.
        hit=trace(world,center+unreal.Vector(0,0,distance),center-unreal.Vector(0,0,distance),ignore)
        assert hit_actor(hit)==guide,(i,'Guide collision missing')
        before=guide.triangle_count;assert guide.mine_sphere(center,120),str(guide.last_status)
        assert guide.successful_cuts==1
        checks.append(dict(index=i+1,chunks=len(guide.chunks),triangles=before,collision=True,excavated=True))
    region.reset_guide_excavation()
    assert all(g.successful_cuts==0 and g.triangle_count==r['triangles'] for g,r in zip(guides,checks))
    paths=json.loads((ART/'centerlines.json').read_text());main=paths[0];floor_point=raw(main['points'][int(len(main['points'])*.4)])
    floor=trace(world,floor_point,floor_point-unreal.Vector(0,0,2000),guides)
    assert hit_actor(floor)==rock
    original_rock=rock.triangle_count;assert rock.mine_sphere(floor.to_tuple()[5],160),str(rock.last_status)
    region.reset_excavation();assert rock.successful_cuts==0 and rock.triangle_count==original_rock
    center=region.get_actor_location();outer=center+unreal.Vector(0,region.rock_size.y*.5+region.shell_thickness+150,0)
    inner=center+unreal.Vector(0,region.rock_size.y*.5+50,0)
    assert hit_actor(trace(world,outer,inner))==shell,'Bedrock casing must remain collision-solid'
    vol=lambda geo:abs(unreal.GeometryScript_MeshQueries.get_mesh_volume_area(geo)[1])
    blocked=sum(vol(source(layer.mesh)) for layer in region.guide_layers)
    total=vol(source(region.cutters[0].mesh));box=region.rock_size.x*region.rock_size.y*region.rock_size.z
    interior=box-vol(rock.get_combined_mesh());assert .45<=blocked/interior<=.55,(blocked,interior)
    for layer in region.guide_layers:
        mesh=source(layer.mesh);_,bvh=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(mesh)
        for path in paths:
            _,inside,_=unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(mesh,bvh,raw(path['points'][-1]),unreal.GeometryScriptSpatialQueryOptions());assert not inside
    mode=world.get_world_settings().get_editor_property('default_game_mode');pawn_cls=unreal.get_default_object(mode).get_editor_property('default_pawn_class')
    assert pawn_cls.get_name()=='BP_MainGameCharacter_C'
    assert unreal.get_default_object(pawn_cls).get_component_by_class(unreal.MiningComponent).fall_reset_distance==0
    report.update(passed=True,map='/Game/MainGame/Maps/Lvl_MainGame',rock_chunks=len(rock.chunks),guides=checks,guide_count=16,
                  guide_chunks=sum(r['chunks'] for r in checks),blocker_fill_fraction=blocked/total,interior_fill_fraction=blocked/interior,
                  rock_excavation_and_reset=True,all_guide_collisions_excavation_reset=True,bedrock_collision=True,all_terminals_clear=True,
                  fall_reset_disabled=True,main_game_character=True)
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
report['elapsed_seconds']=time.monotonic()-started
(ROOT/'Saved/Validation/main_game_reload.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');unreal.log('MAIN_GAME_RELOAD '+json.dumps(report))
