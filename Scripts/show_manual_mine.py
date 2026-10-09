import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningManual_Demo")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningManualRegion)[0]
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([region.main_passage])
unreal.MiningDesignerLibrary.open_manual_editor()
