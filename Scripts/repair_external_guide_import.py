"""Repair micron-size import rounding artifacts while preserving each closed blocker."""
import json,runpy,traceback
from pathlib import Path
import unreal
helpers=runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/external_guide_helpers.py'))
results=[]
try:
    for mesh in helpers['models']():
        dynamic=unreal.DynamicMesh()
        options=unreal.GeometryScriptCopyMeshFromAssetOptions(apply_build_settings=False)
        lod=unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.SOURCE_MODEL,lod_index=0)
        unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(mesh,dynamic,options,lod)
        before=dynamic.get_triangle_count()
        weld=unreal.GeometryScriptWeldEdgesOptions(tolerance=.001,only_unique_pairs=True)
        unreal.GeometryScript_MeshRepair.weld_mesh_edges(dynamic,weld)
        repair=unreal.GeometryScriptDegenerateTriangleOptions(mode=unreal.GeometryScriptRepairMeshMode.REPAIR_OR_SKIP,min_triangle_area=1e-8,min_edge_length=.001)
        unreal.GeometryScript_MeshRepair.repair_mesh_degenerate_geometry(dynamic,repair)
        closed=unreal.GeometryScript_MeshQueries.get_is_closed_mesh(dynamic)
        assert closed,mesh.get_name()
        after=dynamic.get_triangle_count()
        results.append({'mesh':mesh.get_name(),'before_triangles':before,'after_triangles':after,'closed':closed})
        if after!=before:
            unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dynamic,mesh,unreal.GeometryScriptCopyMeshToAssetOptions(),unreal.GeometryScriptMeshWriteLOD())
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/verify_external_guides.py'))
except Exception:
    results.append({'error':traceback.format_exc()});unreal.log_error(str(results[-1]))
finally:
    (Path(unreal.Paths.project_saved_dir())/'Validation'/'external_guide_import_repair.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
