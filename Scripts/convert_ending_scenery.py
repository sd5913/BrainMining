"""One-time migration of the existing ending; repeat runs never rebuild scenery."""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
map_path = '/Game/EndingEmergence/Maps/Lvl_EndingEmergence_Demo'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if (unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
    raise RuntimeError('Use a clean editor; preserve unsaved work.')
assert levels.load_level(map_path)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
demo = next(a for a in actors.get_all_level_actors() if isinstance(a, unreal.EndingEmergence))
if not demo.get_editor_property('editable_scenery'):
    backup = project / 'Saved/Backups' / ('EndingEditable-' + datetime.now().strftime('%Y%m%d-%H%M%S') + '.umap')
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(project / 'Content/EndingEmergence/Maps/Lvl_EndingEmergence_Demo.umap', backup)
    assert demo.convert_scenery_to_editable_actors(), 'Conversion failed; the saved source map is unchanged.'
    assert levels.save_current_level()
    unreal.log('ENDING_EDITABLE_BACKUP=' + str(backup))
editable = [a for a in actors.get_all_level_actors() if a.actor_has_tag('EndingEditableScenery')]
assert len(editable) > 300
assert all(a.get_attach_parent_actor() == demo for a in editable)
# The brain and city share coordinates in different film stages. Apply this
# initial editor view once; later migration runs preserve artist visibility.
if not demo.actor_has_tag('EndingSceneryEditorPreview'):
    components = demo.get_components_by_class(unreal.SceneComponent)
    journey = next(c for c in components if c.get_name().startswith('01_PlayableCellsBloodBrain'))
    city = next(c for c in components if c.get_name().startswith('02_CityAndPeople'))
    journey.set_visibility(False, True)
    city.set_visibility(True, True)
    for c in components:
        if c.get_name().startswith('NightContinentFromClouds'):
            c.set_visibility(False, True)
    for a in editable:
        if a.actor_has_tag('EndingEditableCell'):
            a.root_component.get_attach_parent().set_visibility(True, True)
    demo.set_editor_property('tags', list(demo.tags) + [unreal.Name('EndingSceneryEditorPreview')])
# Editor-only overview cameras make the entire authored scene easy to enter.
# Keep an artist's camera adjustments on subsequent runs.
for tag, label, eye, target in (
    ('EndingSceneryOverviewCity', 'EDIT_街景总览相机', (8000, -15000, 17000), (22000, 0, 0)),
    ('EndingSceneryOverviewCell', 'EDIT_细胞房总览相机', (-180, 0, 260), (1100, 0, 220)),
):
    if not any(a.actor_has_tag(tag) for a in actors.get_all_level_actors()):
        camera = actors.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(*eye))
        camera.set_actor_label(label)
        camera.set_editor_property('tags', [unreal.Name(tag)])
        camera.set_editor_property('is_editor_only_actor', True)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*eye), unreal.Vector(*target)), False)
        camera.camera_component.set_editor_property('field_of_view', 85.0)
        camera.set_folder_path('Ending_Edit/00_Overview')
assert levels.save_current_level()
report = project / 'Saved/Validation/EndingEditable'
report.mkdir(parents=True, exist_ok=True)
(report / 'conversion.json').write_text(json.dumps({
    'map': map_path, 'actors': len(editable),
    'cell': sum(a.actor_has_tag('EndingEditableCell') for a in editable),
    'city': sum(a.actor_has_tag('EndingEditableCity') for a in editable),
    'protected': demo.get_editor_property('editable_scenery'),
}, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('ENDING_EDITABLE_SAVED=' + str(len(editable)))
