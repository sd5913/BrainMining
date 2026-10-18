"""Install the always-loaded subtitle-trigger sublevel in MainGame.

Run in the live editor with PIE stopped and maps saved. Existing subtitle
triggers in MainGame's persistent level move without changing their transforms
or segment IDs. Triggers belonging to referenced source rooms stay there.
"""
import json
from pathlib import Path
import unreal

MAIN = '/Game/MainGame/Maps/Lvl_MainGame'
SUB = '/Game/MainGame/Maps/Lvl_MainGame_SubtitleTriggers'
root = Path(unreal.Paths.project_dir())
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before changing level composition'
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save current maps first'
assert levels.load_level(MAIN), MAIN
world = editor.get_editor_world()
assert levels.set_current_level_by_name('Lvl_MainGame')
persistent = levels.get_current_level()
trigger_class = unreal.load_class(None, '/Script/BrainMining.SubtitleTrigger')

def triggers():
    return unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), trigger_class)

def signature(actor):
    box = actor.get_component_by_class(unreal.BoxComponent)
    return dict(label=actor.get_actor_label(), segment_id=str(actor.get_editor_property('segment_id')),
                location=list(actor.get_actor_location().to_tuple()),
                rotation=list(actor.get_actor_rotation().to_tuple()),
                scale=list(actor.get_actor_scale3d().to_tuple()),
                box_extent=list(box.get_unscaled_box_extent().to_tuple()))

def same_trigger(first, second):
    # Moving between levels uses editor text export, which rounds transforms
    # to six decimal places. Compare below a hundredth of a millimeter.
    return (first['label'] == second['label'] and first['segment_id'] == second['segment_id']
            and all(abs(a - b) < .0001 for key in ('location', 'rotation', 'scale', 'box_extent')
                    for a, b in zip(first[key], second[key])))

before = [signature(a) for a in triggers() if a.get_level() == persistent]
source_triggers = [dict(path=a.get_path_name(), data=signature(a)) for a in triggers()
                   if a.get_level() != persistent and not a.get_level().get_path_name().startswith(SUB + '.')]

stream = unreal.GameplayStatics.get_streaming_level(world, SUB)
if not stream:
    if unreal.EditorAssetLibrary.does_asset_exist(SUB):
        stream = unreal.EditorLevelUtils.add_level_to_world(world, SUB, unreal.LevelStreamingAlwaysLoaded)
    else:
        stream = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, SUB, False)
assert isinstance(stream, unreal.LevelStreamingAlwaysLoaded), 'Sublevel must always load'
loaded = stream.get_loaded_level()
assert loaded and loaded.get_path_name().startswith(SUB + '.')
existing_sublevel = [signature(a) for a in triggers() if a.get_level() == loaded]
transform = stream.get_editor_property('level_transform')
assert all(abs(v) < .0001 for v in transform.translation.to_tuple())
assert all(abs(v) < .0001 for v in transform.rotation.rotator().to_tuple())
assert all(abs(v - 1) < .0001 for v in transform.scale3d.to_tuple())
unreal.EditorLevelUtils.set_level_visibility(loaded, True, False)

to_move = [a for a in triggers() if a.get_level() == persistent]
if to_move:
    assert unreal.EditorLevelUtils.move_actors_to_level(to_move, stream, False, False) == len(before)
after = [signature(a) for a in triggers() if a.get_level() == loaded]
for original in existing_sublevel + before:
    assert any(same_trigger(original, actual) for actual in after), ('Trigger changed during migration', original)
assert len(after) == len(existing_sublevel) + len(before)
assert not [a for a in triggers() if a.get_level() == persistent]

unreal.EditorLevelUtils.make_level_current(stream)
assert unreal.EditorLoadingAndSavingUtils.save_map(loaded.get_outer(), SUB)
assert unreal.EditorLoadingAndSavingUtils.save_map(world, MAIN)
assert levels.load_level(MAIN), MAIN
world = editor.get_editor_world()
stream = unreal.GameplayStatics.get_streaming_level(world, SUB)
assert isinstance(stream, unreal.LevelStreamingAlwaysLoaded) and stream.get_loaded_level()
loaded = stream.get_loaded_level()
saved = [signature(a) for a in triggers() if a.get_level() == loaded]
assert len(saved) == len(after) and all(any(same_trigger(a, b) for b in saved) for a in after)
for original in source_triggers:
    actor = next(a for a in actors.get_all_level_actors() if a.get_path_name() == original['path'])
    assert same_trigger(signature(actor), original['data'])
unreal.EditorLevelUtils.make_level_current(stream)
assert levels.get_current_level() == loaded

report = dict(passed=True, persistent_level=MAIN, subtitle_trigger_level=SUB,
              streaming_class=stream.get_class().get_name(), transform='identity',
              migrated_triggers=before, saved_triggers=saved,
              untouched_source_triggers=source_triggers)
out = root / 'Saved/Validation/SubtitleTriggers'
out.mkdir(parents=True, exist_ok=True)
(out / 'sublevel_created.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('SUBTITLE_TRIGGER_SUBLEVEL_CREATED ' + json.dumps(report, ensure_ascii=False))
