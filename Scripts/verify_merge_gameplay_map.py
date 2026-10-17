"""Read the saved MainGame mineral clues without rewriting the authored map."""
import json
import traceback
from pathlib import Path
import unreal

out = Path(unreal.Paths.project_dir()) / 'Saved/Validation/MergeGameplay'
out.mkdir(parents=True, exist_ok=True)
report = {'passed': False}
try:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert levels.load_level('/Game/MainGame/Maps/Lvl_MainGame')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    fields = [a for a in actors if isinstance(a, unreal.MiningTreasureField) and a.get_editor_property('use_authored_placements')]
    assert len(fields) == 1, f'Expected one authored mineral field, found {len(fields)}'
    field = fields[0]
    mesh = field.get_editor_property('crystal_clue_mesh')
    assert mesh, 'Crystal clue mesh is missing'
    placements = field.get_editor_property('authored_placements')
    assert placements, 'Authored mineral placements are missing'
    fragment_count = 0
    for placement in placements:
        assert placement.fragments, f'Missing clues: {placement.treasure_id} at {placement.transform.translation}'
        assert len(placement.fragments) == len(placement.fragment_supports), 'Clue support count mismatch'
        assert placement.hosts and all(unreal.SystemLibrary.is_valid(h) for h in placement.hosts), 'Missing terrain hosts'
        fragment_count += len(placement.fragments)
    report.update(passed=True, deposits=len(placements), fragments=fragment_count, mesh=mesh.get_path_name(),
                  definitions=[{'id': str(d.id), 'color': str(d.color), 'hint_radius': d.hint_radius} for d in field.definitions])
    report['televisions']=[]
    assert levels.load_level('/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for escape in [a for a in actors if isinstance(a, unreal.MiningRoomEscape)]:
        screen=escape.get_editor_property('authored_television_screen')
        if screen:
            report['televisions'].append({'escape':escape.get_path_name(), 'entry':str(escape.get_editor_property('computer_entry')),
                'screen':screen.get_path_name(), 'location':str(screen.get_actor_location()),
                'rotation':str(screen.get_actor_rotation()), 'scale':str(screen.get_actor_scale3d()),
                'bounds':str(screen.get_actor_bounds(False)), 'rock_size':str(screen.get_editor_property('rock_size'))})
except Exception:
    report['error'] = traceback.format_exc()
finally:
    (out / 'map.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    unreal.SystemLibrary.quit_editor()
