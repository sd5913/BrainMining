"""Refresh the moved soil transition coordinates without touching partitioned geometry."""
import unreal,textwrap,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
source=(ROOT/'Scripts/apply_routes_v5_organic_walls.py').read_text()
scope={'__name__':'material_helpers'}
exec(compile(source[:source.index('\ntry:\n')],'organic_helpers','exec'),scope)
scope['ART']=ROOT/'Art/MainGame/RoutesV6'
body=source[source.index('    clay=unreal.load_asset'):source.index("    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')")]
body=textwrap.dedent(body).replace('for i,stage in enumerate(STAGES):','for i,stage in [(1,STAGES[1])]:')
report={'passed':False}
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Engine/Maps/Entry')
    exec(compile(body,'v6_material_update','exec'),scope);report['passed']=True
except Exception:report['error']=traceback.format_exc();unreal.log_error(report['error'])
finally:
    (ROOT/'Saved/Validation/MainGameRoutesV6/material.json').write_text(json.dumps(report,indent=2));unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(),'QUIT_EDITOR')
