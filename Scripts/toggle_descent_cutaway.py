import unreal


actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
parts = [actor for actor in actors.get_all_level_actors() if actor.actor_has_tag("EditableDescent") and actor.actor_has_tag("EditorCutaway")]
if not parts:
    raise RuntimeError("Open Lvl_MiningDescent_Editable first")
hide = not all(actor.is_temporarily_hidden_in_editor() for actor in parts)
for actor in parts:
    actor.set_is_temporarily_hidden_in_editor(hide)
unreal.log("DESCENT_CUTAWAY " + ("ON" if hide else "OFF"))
