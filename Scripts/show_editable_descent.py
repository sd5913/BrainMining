import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if "Lvl_MiningDescent_Editable" not in editor.get_editor_world().get_path_name():
    raise RuntimeError("Open Lvl_MiningDescent_Editable first")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if actor.actor_has_tag("EditableDescent") and actor.actor_has_tag("EditorCutaway"):
        actor.set_is_temporarily_hidden_in_editor(True)
actors.set_selected_level_actors([])
editor.set_level_viewport_camera_info(unreal.Vector(3100, 4200, 2000), unreal.Rotator(pitch=-29, yaw=-90, roll=0))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("EDITABLE_DESCENT_EDITOR_READY")
