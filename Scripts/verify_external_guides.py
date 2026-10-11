"""Isolated import/generation and excavation test. Does not save the loaded user map."""
import json,time,traceback,runpy
from pathlib import Path
import unreal

output=Path(unreal.Paths.project_saved_dir())/'Validation'/globals().get('GUIDE_REPORT_NAME','external_guides.json')
helpers=runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/external_guide_helpers.py'))
report={'passed':False,'started':time.monotonic()}
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo')
    region=helpers['region_in_current_level']()
    rock,shell=region.generated_rock,region.generated_shell
    meshes=helpers['models']()
    tunnel=unreal.DynamicMesh()
    options=unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False)
    lod=unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(region.cutters[0].mesh,tunnel,options,lod)
    _,tunnel_bvh=unreal.GeometryScript_MeshSpatial.build_bvh_for_mesh(tunnel)
    if globals().get('GENERATE_GUIDES',True):
        helpers['configure'](region,meshes)
        assert region.generate_guide_layers(),str(region.guide_build_status)
    guides=list(region.generated_guide_layers)
    assert len(guides)==20
    assert region.generated_rock==rock and region.generated_shell==shell
    checks=[]
    shared=region.cutters[0].local_transform
    for guide,mesh in zip(guides,meshes):
        assert guide.get_owner()==region
        assert all(c.mesh.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION for c in guide.chunks)
        bounds=mesh.get_bounding_box()
        raw_center=(bounds.min+bounds.max)*.5
        _,inside,_=unreal.GeometryScript_MeshSpatial.is_point_inside_mesh(tunnel,tunnel_bvh,raw_center,unreal.GeometryScriptSpatialQueryOptions())
        assert inside,f'{mesh.get_name()} center does not match imported tunnel coordinates: {raw_center}'
        local_center=unreal.MathLibrary.transform_location(shared,raw_center)
        center=unreal.MathLibrary.transform_location(region.get_actor_transform(),local_center)
        initial=guide.triangle_count
        assert guide.mine_sphere(center,120),str(guide.last_status)
        assert guide.successful_cuts==1
        checks.append({'actor':guide.get_name(),'mesh':mesh.get_name(),'initial_triangles':initial,
                       'chunks':len(guide.chunks),'center_inside_tunnel':inside,'world_center_cm':[center.x,center.y,center.z]})
    region.reset_excavation()
    assert all(g.successful_cuts==0 and g.triangle_count==c['initial_triangles'] for g,c in zip(guides,checks))
    assert region.generated_rock==rock and region.generated_shell==shell
    report.update(passed=True,count=len(guides),status=str(region.guide_build_status),guides=checks,
                  all_twenty_excavated_and_reset=True,base_rock_and_shell_preserved=True,
                  generation_performed=globals().get('GENERATE_GUIDES',True))
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    report['elapsed_seconds']=time.monotonic()-report['started'];output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('EXTERNAL_GUIDES '+json.dumps(report,ensure_ascii=False))
