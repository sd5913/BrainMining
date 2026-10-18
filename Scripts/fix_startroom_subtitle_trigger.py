"""Align the subtitle sublevel's opening trigger with MainGame's wake-up landing.

Run through the editor Python console with PIE stopped. Saves only the subtitle
sublevel; the persistent map and the source bedroom are not changed.
"""
import unreal

MAIN = '/Game/MainGame/Maps/Lvl_MainGame'
SUB = MAIN + '_SubtitleTriggers'
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert not editor.get_game_world(), 'Stop PIE before editing the trigger'
world = editor.get_editor_world()
assert world.get_path_name().split('.')[0] == MAIN, 'Open MainGame first'
stream = unreal.GameplayStatics.get_streaming_level(world, SUB)
assert stream and stream.get_loaded_level(), 'Load the subtitle sublevel first'
level = stream.get_loaded_level()
all_actors = actors.get_all_level_actors()
triggers = [a for a in all_actors if isinstance(a, unreal.SubtitleTrigger)
            and a.get_level() == level
            and str(a.get_editor_property('segment_id')) == 'StartRoom']
assert len(triggers) == 1, 'Expected exactly one StartRoom subtitle trigger'
room = next(a for a in all_actors if isinstance(a, unreal.MiningStartRoom))
# EnsureMainGameOpening uses these local coordinates when there is no authored
# opening actor. Respect an explicit opening's landing if one is already placed.
opening = next((a for a in all_actors if isinstance(a, unreal.MiningOpening)), None)
landing = (opening.get_editor_property('standing_location') if opening else
           room.get_actor_transform().transform_location(unreal.Vector(-260, -70, 98)))
rotation = room.get_actor_rotation()
trigger = triggers[0]
box = trigger.get_component_by_class(unreal.BoxComponent)
with unreal.ScopedEditorTransaction('Align StartRoom subtitle with opening landing'):
    trigger.modify()
    box.modify()
    trigger.set_actor_location(landing, False, False)
    trigger.set_actor_rotation(rotation, False)
    trigger.set_actor_scale3d(unreal.Vector(1, 1, 1))
    # A narrow strip across the standing location stays clear of the initial
    # PlayerStart and seated menu pose. The opening disables pawn collision
    # during its animation and restores it at this landing, producing the entry.
    box.set_box_extent(unreal.Vector(180, 25, 160))
    box.set_collision_profile_name('Trigger')
    box.set_editor_property('generate_overlap_events', True)
    trigger.set_actor_enable_collision(True)
    trigger.set_folder_path('Subtitles/PositionTriggers')
unreal.EditorLevelUtils.make_level_current(stream)
assert unreal.EditorLoadingAndSavingUtils.save_map(level.get_outer(), SUB), 'Save failed'
unreal.log('STARTROOM_SUBTITLE_REPAIRED location=' + str(landing) + ' saved=' + SUB)
