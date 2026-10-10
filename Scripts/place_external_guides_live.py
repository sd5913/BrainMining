"""Place the tested guide meshes with the tool and save the user's current test map."""
import json,time,traceback,runpy
from pathlib import Path
import unreal

output=Path(unreal.Paths.project_saved_dir())/'Validation'/'external_guides_live.json'
state={'passed':False,'phase':'queued','started':time.monotonic()}
def checkpoint():output.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
def tick(delta):
    if time.monotonic()-state['started']<2:return
    unreal.unregister_slate_post_tick_callback(callback)
    try:
        editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        assert editor.get_game_world() is None
        assert editor.get_editor_world().get_path_name()=='/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo.Lvl_ExternalMining_Demo'
        helpers=runpy.run_path(str(Path(unreal.Paths.project_dir())/'Scripts/external_guide_helpers.py'))
        region=helpers['region_in_current_level']()
        rock,shell=region.generated_rock,region.generated_shell
        helpers['configure'](region,helpers['models']())
        state['phase']='generating';checkpoint()
        assert region.generate_guide_layers(),str(region.guide_build_status)
        assert region.generated_rock==rock and region.generated_shell==shell
        state.update(count=len(region.generated_guide_layers),status=str(region.guide_build_status),phase='saving')
        checkpoint();assert state['count']==20
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([region])
        state.update(passed=True,phase='complete',saved=True)
    except Exception:
        state.update(phase='failed',error=traceback.format_exc());unreal.log_error(state['error'])
    finally:
        state['elapsed_seconds']=time.monotonic()-state['started'];checkpoint()
        unreal.log('EXTERNAL_GUIDES_LIVE '+json.dumps(state,ensure_ascii=False))
checkpoint()
callback=unreal.register_slate_post_tick_callback(tick)
