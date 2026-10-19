"""Exercise save/reload under Run_EndingEditable's byte-for-byte backup/finally restore."""
import json
import hashlib
import runpy
from pathlib import Path
import unreal

MAP = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if (unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
    raise RuntimeError('Use a clean editor.')
source_file = Path(unreal.Paths.project_content_dir()) / 'EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap'
source_hash = hashlib.sha256(source_file.read_bytes()).hexdigest()
backup = Path(unreal.Paths.project_saved_dir()) / 'Validation/EndingEditable/Verify/source-before-test.umap'
assert backup.exists() and hashlib.sha256(backup.read_bytes()).hexdigest() == source_hash, 'Run through Run_EndingEditable.ps1 -Mode Verify so the original map is restored.'
assert levels.load_level(MAP)
def editable():
    return [a for a in actors.get_all_level_actors() if a.actor_has_tag('EndingEditableScenery')]
def controller():
    return next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.EndingEmergence))
def location(actor):
    p = actor.get_actor_location()
    return (p.x, p.y, p.z)
def transform(actor):
    r, s = actor.get_actor_rotation(), actor.get_actor_scale3d()
    return location(actor) + (r.pitch, r.yaw, r.roll, s.x, s.y, s.z)

all_before = editable()
assert all(a.root_component.is_visible() for a in all_before), 'Authored cell/street must be visible together in the editor.'
assert all(not c.is_visible() for c in controller().get_components_by_class(unreal.SceneComponent)
           if c.get_name().startswith(('01_PlayableCellsBloodBrain', 'NightContinentFromClouds'))), 'Other film stages must not obscure the editor street.'
building = next(a for a in all_before if 'CityAssetBuildings' in a.get_actor_label())
cell = next(a for a in all_before if 'CellDressNucleus_' in a.get_actor_label())
deleted = next(a for a in all_before if 'CityDressProp' in a.get_actor_label())
deleted_label = deleted.get_actor_label()
assert actors.destroy_actor(deleted)
building.set_actor_location(building.get_actor_location() + unreal.Vector(131, -57, 43), False, False)
cell.set_actor_location(cell.get_actor_location() + unreal.Vector(23, 19, 37), False, False)
cell.set_actor_scale3d(cell.get_actor_scale3d() * 1.15)
building.set_actor_rotation(unreal.Rotator(0, 37, 0), False)
mesh = building.static_mesh_component
replacement = unreal.load_asset('/Engine/BasicShapes/Cube')
material = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
mesh.set_static_mesh(replacement)
mesh.set_material(0, material)
duplicate = actors.duplicate_actor(building, building.get_world(), unreal.Vector(240, 0, 0))
assert duplicate
assert duplicate.root_component.get_attach_parent() == building.root_component.get_attach_parent(), 'Duplication lost its animation/stage anchor.'
expected = {a.get_name(): transform(a)
            for a in (building, cell, duplicate)}
anchors = {a.get_name(): a.root_component.get_attach_parent().get_name() for a in (building, cell, duplicate)}
building_name = building.get_name()
assert levels.save_current_level()
assert levels.load_level('/Engine/Maps/Entry')
assert levels.load_level(MAP)
demo = controller()
count = len(editable())
assert count == len(all_before)
assert demo.get_editor_property('editable_scenery')
demo.build_demo()
demo.apply_city_buildings()
demo.apply_cell_visuals()
assert demo.convert_scenery_to_editable_actors()
assert len(editable()) == count, 'Re-running migration duplicated or restored deleted scenery.'
by_name = {a.get_name(): a for a in editable()}
# UE may reuse a destroyed actor's object name for the newly duplicated actor.
assert all(a.get_actor_label() != deleted_label for a in by_name.values())
for name, value in expected.items():
    a = by_name[name]
    assert all(abs(x-y) < 0.001 for x, y in zip(transform(a), value)), (name, value, transform(a))
    assert a.get_attach_parent_actor() == demo, 'Duplication/reload lost stage attachment.'
    assert a.root_component.get_attach_parent().get_name() == anchors[name]
assert by_name[building_name].static_mesh_component.static_mesh == replacement
assert by_name[building_name].static_mesh_component.get_material(0) == material
assert levels.save_current_level()
assert levels.load_level(MAP)
# Each legacy entry must stop before modifying materials or the authored map.
protected = []
for name in ('create_ending_emergence.py', 'apply_ending_city.py', 'apply_ending_cell.py',
             'apply_ending_organic_art.py', 'refresh_ending_rooms.py'):
    try:
        runpy.run_path(str(Path(__file__).with_name(name)))
    except RuntimeError as exc:
        assert 'ENDING_SCENERY_PROTECTED' in str(exc), str(exc)
        protected.append(name)
    else:
        raise AssertionError('Legacy script bypassed authoring protection: ' + name)
report = Path(unreal.Paths.project_saved_dir()) / 'Validation/EndingEditable/editor-verification.json'
report.write_text(json.dumps({'passed': True, 'actors': count, 'save_reload': True,
    'move_rotate_scale': True, 'replace_mesh_material': True, 'duplicate_delete': True,
    'idempotent_conversion': True, 'source_map_sha256': source_hash,
    'protected_scripts': protected}, indent=2), encoding='utf-8')
unreal.log('ENDING_EDITABLE_EDITOR_VERIFIED')
