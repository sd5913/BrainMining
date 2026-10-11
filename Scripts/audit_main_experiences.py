import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for path in ['/Game/Showcase/Maps/Lvl_ContinuousShowcase', '/Game/Showcase/Maps/Lvl_MainExperiences']:
    levels.load_level(path)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.MiningTreasureField):
            for name in ['treasure_material', 'rock_material', 'guide_material']:
                value = actor.get_editor_property(name)
                unreal.log('AREA_AUDIT ' + path + ' ' + name + '=' + str(value))
        if isinstance(actor, unreal.GeneratedMiningRock) and 'BM_Mining' in [str(tag) for tag in actor.tags]:
            unreal.log('AREA_AUDIT MINE ' + actor.get_actor_label() + ' ' + str(actor.get_actor_location()))
