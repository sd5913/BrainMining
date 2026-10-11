"""Deferred live-editor regeneration of the saved user's current external mine."""
import json,time,traceback
from pathlib import Path
import unreal

output=Path(unreal.Paths.project_saved_dir())/'Validation'/'external_budget_8192_live.json'
state={'phase':'queued','passed':False,'started':time.monotonic()}

def checkpoint():
    output.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')

def tick(delta):
    if time.monotonic()-state['started']<2:return
    unreal.unregister_slate_post_tick_callback(callback)
    try:
        editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        assert editor.get_game_world() is None,'PIE must be stopped'
        world=editor.get_editor_world()
        assert world.get_path_name()=='/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo.Lvl_ExternalMining_Demo'
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        region=next(a for a in actors.get_all_level_actors() if isinstance(a,unreal.MiningExternalRegion) and a.get_name()=='MiningExternalRegion_1')
        state.update(phase='generating',rock_size_cm=[region.rock_size.x,region.rock_size.y,region.rock_size.z],chunk_length_cm=region.chunk_length)
        checkpoint();assert region.generate_region(),str(region.build_status)
        rock=region.generated_rock
        state.update(chunks=len(rock.chunks),largest_chunk_triangles=rock.largest_chunk_triangles,
                     triangles=rock.triangle_count,build_status=str(region.build_status),phase='saving')
        checkpoint()
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        state.update(passed=True,phase='complete',saved=True)
    except Exception:
        state.update(phase='failed',error=traceback.format_exc());unreal.log_error(state['error'])
    finally:
        state['elapsed_seconds']=time.monotonic()-state['started'];checkpoint()
        unreal.log('EXTERNAL_BUDGET_8192_LIVE '+json.dumps(state,ensure_ascii=False))

checkpoint()
callback=unreal.register_slate_post_tick_callback(tick)
