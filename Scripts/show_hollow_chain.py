import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None, "Stop Play before opening the editable example"
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningHollowChain_Demo")
generator = unreal.GameplayStatics.get_all_actors_of_class(editor.get_editor_world(), unreal.MiningTunnelGenerator)[0]
actors.set_selected_level_actors([generator])
unreal.MiningDesignerLibrary.open_designer()
