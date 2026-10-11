"""Isolated check of the current widened-tunnel configuration; never save this temporary map."""
import json,time,traceback
from pathlib import Path
import unreal

output=Path(unreal.Paths.project_saved_dir())/'Validation'
output.mkdir(parents=True,exist_ok=True)
report={'passed':False,'started':time.time(),'limit':8192,'expected_grid_cells':3360}

def checkpoint():
    (output/'external_wide_capacity.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

try:
    assert unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh=unreal.load_asset('/Game/ExternalMining/Model/SM_DescendingTunnel_Wide_Lowpoly')
    assert mesh,'Widened tunnel asset missing'
    region=actors.spawn_actor_from_class(unreal.MiningExternalRegion,unreal.Vector())
    size=unreal.Vector(30943.187501,21456.30882,19130.803617)
    region.set_editor_property('rock_size',size)
    region.set_editor_property('chunk_length',1600)
    region.set_editor_property('show_cutter_preview',False)
    cutter=unreal.MiningExternalCutter()
    cutter.set_editor_property('mesh',mesh)
    cutter.set_editor_property('local_transform',unreal.Transform(location=unreal.Vector(1016.2234805278022,2387.211116886465,1898.769194377222)))
    region.set_editor_property('cutters',[cutter])
    report.update(asset=mesh.get_path_name(),rock_size_cm=[size.x,size.y,size.z],chunk_length_cm=1600,phase='generate')
    checkpoint();started=time.monotonic()
    assert region.generate_region(),str(region.build_status)
    rock=region.generated_rock
    assert rock and region.generated_shell
    assert 2048<len(rock.chunks)<=8192
    assert len(rock.chunks)==3360
    assert all(c.initial_mesh.get_triangle_count()>0 for c in rock.chunks)
    report.update(generation_seconds=time.monotonic()-started,chunks=len(rock.chunks),
                  triangles=rock.triangle_count,largest_chunk_triangles=rock.largest_chunk_triangles,
                  build_status=str(region.build_status),phase='dig_and_reset')
    checkpoint();initial=rock.triangle_count
    assert rock.mine_sphere(unreal.Vector(size.x*.5-40,0,0),120),str(rock.last_status)
    report['dig_touched_chunks']=rock.last_touched_chunks
    region.reset_excavation()
    assert rock.triangle_count==initial and rock.successful_cuts==0
    report.update(passed=True,phase='complete',dig_and_reset=True)
except Exception:
    report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    report['elapsed_seconds']=time.time()-report['started'];checkpoint()
    unreal.log('EXTERNAL_WIDE_CAPACITY '+json.dumps(report,ensure_ascii=False))
