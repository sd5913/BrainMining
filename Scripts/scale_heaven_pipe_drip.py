import unreal

MAP = '/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet'
LABEL = '入口露出管口 · 持续滴水'
FACTOR = unreal.Vector(2.5, 2.5, 2.5)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.load_level(MAP):
    raise RuntimeError('Could not load ' + MAP)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
matched = []
for actor in actors:
    if actor.get_actor_label() != LABEL:
        continue
    old = actor.get_actor_scale3d()
    actor.set_actor_scale3d(unreal.Vector(old.x * FACTOR.x, old.y * FACTOR.y, old.z * FACTOR.z))
    matched.append({'actor': actor.get_path_name(), 'old': str(old), 'new': str(actor.get_actor_scale3d())})
if not matched:
    raise RuntimeError('No actor matched: ' + LABEL)
unreal.EditorLevelLibrary.save_current_level()
print('HEAVEN_PIPE_DRIP_SCALED', matched)
