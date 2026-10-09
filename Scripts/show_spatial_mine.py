import unreal


levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert editor.get_game_world() is None
assert levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningSpatial_Demo")
world = editor.get_editor_world()
region = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningSpatialRegion)[0]
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.show_layout()
actors.set_selected_level_actors([region.main_tunnel])
editor.set_level_viewport_camera_info(unreal.Vector(1200, -5800, 4000), unreal.Rotator(pitch=-29, yaw=67, roll=0))
unreal.MiningDesignerLibrary.open_spatial_editor()
unreal.log("SPATIAL_EDITOR_READY " + generator.last_build_status)
