"""Read-only final editor state and retained validation results."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/Validation/TraversalFeedback'
E=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=E.get_editor_world();A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=list(A.get_all_level_actors())
report={'editor_world':world.get_path_name(),'pie_running':E.get_game_world() is not None,
        'heaven_buffer':any(a.get_actor_label()=='COL_Heaven_OuterDiggableBuffer' for a in actors),
        'heaven_boundary':any(a.get_actor_label()=='COL_Heaven_OuterUnmineableBoundary' for a in actors),
        'subtitle_triggers_loaded':any('Lvl_MainGame_SubtitleTriggers' in a.get_level().get_path_name() for a in actors),
        'checks':{}}
for name in ['Opening/runtime.json','runtime.json','feather.json','Blood/runtime.json','Ending/runtime.json','cook-verification.json','assets.json','heaven.json','nebula_assets.json']:
    data=json.loads((out/name).read_text(encoding='utf-8'))
    report['checks'][name]=bool(data.get('passed'))
assert all(report['checks'].values()),report
assert report['editor_world'].startswith('/Game/MainGame/Maps/Lvl_MainGame.'),report
assert report['heaven_buffer'] and report['heaven_boundary'] and report['subtitle_triggers_loaded'],report
(out/'final-state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('FEEDBACK_FINAL_STATE '+json.dumps(report))
