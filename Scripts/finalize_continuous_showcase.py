import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Showcase/Maps/Lvl_ContinuousShowcase')
wall = unreal.load_asset('/Game/Rooms/CardDuel/Materials/M_Cream')
floor = unreal.load_asset('/Game/Showcase/Materials/M_ContinuousFloor')
for actor in actors.get_all_level_actors():
    position = actor.get_actor_location()
    if type(actor) == unreal.Actor and abs(position.y - 6500) < 500:
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            component.set_material(0, floor if actor.get_actor_label() == 'Floor' else wall)
assert levels.save_current_level()
unreal.log('CONTINUOUS_MATERIALS_FINALIZED')
