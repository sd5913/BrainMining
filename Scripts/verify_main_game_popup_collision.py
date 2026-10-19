"""PIE regression checks for the popup room's obsolete furniture collision."""
import json
from pathlib import Path
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'Lvl_MainGame' in world.get_path_name(), 'Run MainGame in PIE'
stage = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PopupCloseStage)[0]
origin = stage.get_actor_location()
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
checks = []

def sweep(name, start, end, expected_component=None, clear=False):
    hit = unreal.SystemLibrary.capsule_trace_single_by_profile(
        world, origin + unreal.Vector(*start), origin + unreal.Vector(*end),
        34, 88, 'Pawn', False, [pawn], unreal.DrawDebugTrace.NONE)
    values = hit.to_tuple() if hit else None
    blocking = bool(values and values[0])
    component = values[10] if blocking else None
    passed = not blocking if clear else bool(component and component.get_name() == expected_component)
    checks.append(dict(name=name, passed=passed, blocking=blocking,
                       actor=values[9].get_path_name() if blocking and values[9] else None,
                       component=component.get_name() if component else None,
                       distance=values[3] if blocking else None))

# These lanes previously intersected hulls belonging to distant desks.
sweep('West aisle no longer blocked by distant desk', (-800, 0, 98), (-400, 0, 98), clear=True)
sweep('Central aisle no longer blocked by central desk hull', (-350, 190, 98), (150, 190, 98), clear=True)
sweep('Floor still supports player', (-700, 0, 160), (-700, 0, -100), 'Floor')
sweep('North wall still blocks player above nearby chairs', (0, 650, 260), (0, 820, 260), 'WallPositiveY')
sweep('Closed bomb door still blocks player', (870, 0, 98), (1080, 0, 98), 'OfficeBlastDoor')
sweep('Visible office desk still blocks player', (-448, -325, 240), (-448, -325, 90), 'StaticMeshComponent0')

counts = {}
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    if actor.get_level() != stage.get_level():
        continue
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if mesh and mesh.get_name() in ('SM_OfficeDesk', 'SM_DeskChair', 'SM_Copier'):
            key = mesh.get_name()
            counts[key] = counts.get(key, 0) + 1
for name in ('SM_OfficeDesk', 'SM_DeskChair', 'SM_Copier'):
    checks.append(dict(name=name + ' loaded from updated shared asset', passed=counts.get(name, 0) > 0,
                       instances=counts.get(name, 0)))
for office in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PopupCloseOffice):
    if office.get_level() == stage.get_level():
        checks.append(dict(name='Procedural office hulls remain disabled', passed=all(
            c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
            for c in office.get_components_by_class(unreal.InstancedStaticMeshComponent))))

report = dict(passed=all(c['passed'] for c in checks), world=world.get_path_name(),
              source_level=stage.get_level().get_path_name(), checks=checks)
out = Path(unreal.Paths.project_dir()) / 'Saved/Validation/PopupCollisionAudit'
out.mkdir(parents=True, exist_ok=True)
(out / 'main_game_regression.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('MAIN_GAME_POPUP_COLLISION ' + json.dumps(report))
assert report['passed'], report

