"""Reopen the saved map and verify removal, recipes, hosts, and the exit seal."""
import unreal
import json
import sys
import traceback
from collections import Counter
from pathlib import Path
ROOT = Path(unreal.Paths.project_dir()).resolve()
sys.path.insert(0, str(ROOT / 'Scripts'))
import campaign_minerals_api as api
OUT = ROOT / 'Saved/Validation/MaterialProgression'
report = {'passed': False}
ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
try:
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/MainGame/Maps/Lvl_MainGame')
    world, hall, actors = api._context()
    field = api.get_field(hall)
    assert len(hall.areas) == 6
    assert str(hall.areas[-1].tag) == 'BM_Campaign_MainGameMuseum'
    assert not hall.areas[-1].progress_gate
    assert set(str(d.id) for d in field.definitions) == {'2','4','6','9'}
    allowed = {'BM_Campaign_MainGame01':{'2','4'}, 'BM_Campaign_MainGame02':{'2','4','6'}, 'BM_Campaign_MainGame03':{'2','4','6','9'}}
    counts = {tag:Counter() for tag in allowed}
    for p in field.get_editor_property('authored_placements'):
        assert str(p.treasure_id) in allowed[str(p.area_tag)]
        assert all(unreal.SystemLibrary.is_valid(h) for h in p.hosts)
        counts[str(p.area_tag)][str(p.treasure_id)] += 1
    for tag, c in counts.items():
        assert set(c) == allowed[tag]
        values = [c[k] for k in sorted(c, key=int)]
        assert values == sorted(values, reverse=True)
    all_actors = actors.get_all_level_actors()
    assert not any(a.actor_has_tag('BM_Campaign_MainGame04') or a.actor_has_tag('BM_MainGame_NeuralV4') for a in all_actors)
    wall = next(a for a in all_actors if a.get_actor_label() == 'MAIN_GAME_Museum_TerminusWall')
    assert not isinstance(wall, unreal.DiggableRock)
    pos = wall.get_actor_location()
    hits = 0
    for y in (-180,0,180):
        for z in (-150,0,150):
            hit = unreal.SystemLibrary.line_trace_single(world, pos+unreal.Vector(-100,y,z), pos+unreal.Vector(100,y,z), unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
            assert hit and hit.to_tuple()[0] and hit.to_tuple()[9] == wall
            hits += 1
    validation = api.validate(field, hall)
    assert validation['ok'], validation.get('errors')
    report.update(passed=True, areas=6, mines=3, terminal_room='Museum', counts=counts, seal_collision_samples=hits, minerals=validation)
except Exception:
    report['error'] = traceback.format_exc()
    unreal.log_error(report['error'])
finally:
    (OUT / 'saved.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.SystemLibrary.execute_console_command(ed.get_editor_world(), 'QUIT_EDITOR')
