"""Isolated capacity validation using the actual saved Blender tunnel asset.

Never run in the user's working editor: this creates an unsaved test map.
"""
import json
import time
import traceback
from pathlib import Path
import unreal

output = Path(unreal.Paths.project_saved_dir())/'Validation'
output.mkdir(parents=True,exist_ok=True)
report = {'passed':False,'started':time.time()}

def checkpoint():
    (output/'external_budget_real_tunnel.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    mesh = unreal.load_asset('/Game/ExternalMining/Model/SM_DescendingTunnel_Cutter_Lowpoly')
    assert mesh
    bounds = mesh.get_bounding_box()
    minimum,maximum = bounds.min,bounds.max
    center = (minimum+maximum)*.5
    size = maximum-minimum+unreal.Vector(600,600,600)
    region = actors.spawn_actor_from_class(unreal.MiningExternalRegion,unreal.Vector())
    region.set_editor_property('rock_size',size)
    region.set_editor_property('chunk_length',1600)
    region.set_editor_property('show_cutter_preview',False)
    cutter = unreal.MiningExternalCutter()
    cutter.set_editor_property('mesh',mesh)
    cutter.set_editor_property('local_transform',unreal.Transform(location=-center))
    region.set_editor_property('cutters',[cutter])
    report.update(asset=mesh.get_path_name(),rock_size_cm=[size.x,size.y,size.z],
                  triangle_budget=region.chunk_triangle_budget,phase='generate_full_route')
    checkpoint()
    started = time.monotonic()
    assert region.generate_region(),str(region.build_status)
    rock = region.generated_rock
    assert rock and region.generated_shell
    assert region.chunk_triangle_budget == rock.max_triangles == 100000
    assert 128 < len(rock.chunks) <= 8192
    assert all(c.initial_mesh.get_triangle_count()>0 for c in rock.chunks)
    report.update(phase='dig_and_reset',generation_seconds=time.monotonic()-started,
                  chunks=len(rock.chunks),initial_triangles=rock.triangle_count,
                  largest_chunk_triangles=rock.largest_chunk_triangles,build_status=str(region.build_status))
    checkpoint()
    initial = rock.triangle_count
    assert rock.mine_sphere(unreal.Vector(size.x*.5-40,0,0),120),str(rock.last_status)
    report['dig_touched_chunks'] = rock.last_touched_chunks
    region.reset_excavation()
    assert rock.triangle_count == initial and rock.successful_cuts == 0
    report.update(passed=True,phase='complete',dig_and_reset=True)
    # Also reproduce the dimensions and cutter placement saved in the user's
    # current test map, without loading or modifying that map.
    current = actors.spawn_actor_from_class(unreal.MiningExternalRegion,unreal.Vector(100000,0,0))
    current.set_editor_property('rock_size',unreal.Vector(5706.969214,7456.694172,5418.992369))
    current.set_editor_property('show_cutter_preview',False)
    current_cutter = unreal.MiningExternalCutter()
    current_cutter.set_editor_property('mesh',mesh)
    current_cutter.set_editor_property('local_transform',unreal.Transform(location=unreal.Vector(1016.223481,2438.671439,910.651674)))
    current.set_editor_property('cutters',[current_cutter])
    current_passed = current.generate_region()
    report['current_test_configuration'] = {'passed':current_passed,'status':str(current.build_status)}
    if current_passed:
        report['current_test_configuration']['chunks'] = len(current.generated_rock.chunks)
    checkpoint()
except Exception:
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    report['elapsed_seconds'] = time.time()-report['started']
    checkpoint()
    unreal.log('EXTERNAL_BUDGET_REAL_TUNNEL '+json.dumps(report,ensure_ascii=False))
