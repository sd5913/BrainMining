"""Import, verify, and replace only the guide layers using the independent generator."""
import json,runpy,time,traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir())
ART=ROOT/'Art/Mining/TunnelFill50V3'
DEST='/Game/ExternalMining/GuideLayers/Fill50V3'
MAP='/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo'
helpers=runpy.run_path(str(ROOT/'Scripts/external_guide_helpers.py'))
namespace=helpers['models'].__globals__
namespace['DEST']=DEST;namespace['SOURCE']=ART/'Tunnel_Fill50_Blockers_20.fbx'
lod=unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0)
options=unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False)
plan=json.loads((ART/'placement_plan.json').read_text())['objects']
model_report=json.loads((ART/'validation.json').read_text())

def raw(point):return unreal.Vector(point[0]*100,-point[1]*100,point[2]*100)

def dynamic(mesh):
    result=unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,result,options,lod)
    return result

def import_models():
    meshes=helpers['models']()
    repairs=[]
    for mesh in meshes:
        geo=dynamic(mesh);before=geo.get_triangle_count()
        unreal.GeometryScript_MeshRepair.weld_mesh_edges(geo,unreal.GeometryScriptWeldEdgesOptions(tolerance=.001,only_unique_pairs=True))
        unreal.GeometryScript_MeshRepair.repair_mesh_degenerate_geometry(geo,
            unreal.GeometryScriptDegenerateTriangleOptions(mode=unreal.GeometryScriptRepairMeshMode.REPAIR_OR_SKIP,min_triangle_area=1e-8,min_edge_length=.001))
        assert unreal.GeometryScript_MeshQueries.get_is_closed_mesh(geo),mesh.get_name()
        after=geo.get_triangle_count()
        if after!=before:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(geo,mesh,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        repairs.append(dict(mesh=mesh.get_name(),before=before,after=after))
    (ROOT/'Saved/Validation/fill50_import.json').write_text(json.dumps(repairs,indent=2),encoding='utf-8')
    return meshes

def check(region,meshes):
    guides=list(region.generated_guide_layers);assert len(guides)==20
    tunnel=dynamic(region.cutters[0].mesh);_,tree=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(tunnel)
    source_volume=abs(unreal.GeometryScript_MeshQueries.get_mesh_volume_area(tunnel)[1])
    blocked_volume=sum(abs(unreal.GeometryScript_MeshQueries.get_mesh_volume_area(dynamic(m))[1]) for m in meshes)
    ratio=blocked_volume/source_volume;assert abs(ratio-.5)<.015,ratio
    shared=region.cutters[0].local_transform;transform=region.get_actor_transform()
    checks=[]
    for guide,mesh,record in zip(guides,meshes,plan):
        center=raw(record['center_m'])
        _,inside,_=unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(tunnel,tree,center,unreal.GeometryScriptSpatialQueryOptions())
        assert inside,(mesh.get_name(),center)
        assert guide.get_owner()==region
        assert all(c.mesh.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for c in guide.chunks)
        world=unreal.MathLibrary.transform_location(transform,unreal.MathLibrary.transform_location(shared,center))
        before=guide.triangle_count
        assert guide.mine_sphere(world,120),str(guide.last_status)
        assert guide.successful_cuts==1
        checks.append(dict(actor=guide.get_name(),mesh=mesh.get_name(),triangles=before,chunks=len(guide.chunks),center_inside_tunnel=True))
    region.reset_guide_excavation()
    assert all(g.successful_cuts==0 and g.triangle_count==c['triangles'] for g,c in zip(guides,checks))
    # Imported coordinates and unit conversion must also preserve empty path endpoints.
    paths=json.loads((ROOT/'Art/Mining/TunnelWideV2/centerlines.json').read_text())
    for mesh in meshes:
        geo=dynamic(mesh);_,bvh=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(geo)
        for path in paths:
            _,inside,_=unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(geo,bvh,raw(path['points'][-1]),unreal.GeometryScriptSpatialQueryOptions())
            assert not inside,(mesh.get_name(),path['name'],'terminal covered')
    return dict(passed=True,count=20,fill_fraction=ratio,guide_chunks=sum(c['chunks'] for c in checks),
                all_twenty_excavated_and_reset=True,all_eight_terminals_unblocked=True,guides=checks)

def verify(reload=False):
    output=ROOT/'Saved/Validation'/('fill50_reload.json' if reload else 'fill50_verified.json')
    result={'passed':False};started=time.monotonic()
    try:
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
        region=helpers['region_in_current_level']();rock,shell=region.generated_rock,region.generated_shell
        meshes=helpers['models']() if reload else import_models()
        if not reload:
            helpers['configure'](region,meshes)
            assert region.generate_guide_layers(),str(region.guide_build_status)
        result.update(check(region,meshes),generation_performed=not reload)
        assert region.generated_rock==rock and region.generated_shell==shell
        result['base_preserved']=True
    except Exception:result['error']=traceback.format_exc();unreal.log_error(result['error'])
    result['elapsed_seconds']=time.monotonic()-started
    output.write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.log('FILL50_VERIFY '+json.dumps(result))

def replace_live():
    output=ROOT/'Saved/Validation/fill50_live.json'
    state={'passed':False,'phase':'queued','started':time.monotonic()}
    def write():output.write_text(json.dumps({k:v for k,v in state.items() if k!='callback'},ensure_ascii=False,indent=2),encoding='utf-8')
    def tick(delta):
        if time.monotonic()-state['started']<2:return
        unreal.unregister_slate_post_tick_callback(state['callback'])
        try:
            editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
            assert editor.get_game_world() is None
            assert editor.get_editor_world().get_path_name()==MAP+'.Lvl_ExternalMining_Demo'
            verified=json.loads((ROOT/'Saved/Validation/fill50_verified.json').read_text());assert verified['passed']
            registry=unreal.AssetRegistryHelpers.get_asset_registry();registry.scan_paths_synchronous([DEST],force_rescan=True)
            region=helpers['region_in_current_level']();rock,shell=region.generated_rock,region.generated_shell
            old_names=[g.get_name() for g in region.generated_guide_layers]
            old_config=list(region.guide_layers);old_follow=region.guide_follow_cutter_transform;old_index=region.guide_cutter_index
            helpers['configure'](region,helpers['models']())
            state['phase']='replacing';write()
            if not region.generate_guide_layers():
                region.set_editor_property('guide_layers',old_config);region.set_editor_property('guide_follow_cutter_transform',old_follow);region.set_editor_property('guide_cutter_index',old_index)
                raise RuntimeError(str(region.guide_build_status))
            assert region.generated_rock==rock and region.generated_shell==shell
            actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            owned=[a for a in actors.get_all_level_actors() if a.get_owner()==region and a.actor_has_tag('ExternalMiningGuideLayer')]
            assert len(owned)==20 and not any(a.get_name() in old_names for a in owned)
            state.update(count=20,guide_chunks=sum(len(g.chunks) for g in region.generated_guide_layers),old_names=old_names,
                         new_names=[g.get_name() for g in region.generated_guide_layers],base_preserved=True,
                         status=str(region.guide_build_status),fill_fraction=verified['fill_fraction'],phase='saving')
            write();assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
            actors.set_selected_level_actors([region]);state.update(passed=True,saved=True,phase='complete')
        except Exception:state.update(phase='failed',error=traceback.format_exc());unreal.log_error(state['error'])
        state.pop('callback',None);state['elapsed_seconds']=time.monotonic()-state['started'];write()
    write();state['callback']=unreal.register_slate_post_tick_callback(tick)

if __name__=='__main__':verify()
