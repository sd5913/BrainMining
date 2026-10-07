import unreal


editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_editor_world()
assert "Lvl_MiningDescent_AutoRooms" in world.get_path_name(), "Open Lvl_MiningDescent_AutoRooms first"
generator = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningTunnelGenerator)[0]
generator.set_editor_property("show_bedrock_in_editor", False)
rooms = sorted(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.MiningLayoutRoom), key=lambda actor: actor.connection_order)
unreal.get_editor_subsystem(unreal.EditorActorSubsystem).set_selected_level_actors([rooms[1] if len(rooms) > 1 else rooms[0]])
locations = [room.get_actor_location() for room in rooms]
middle = (min(position.x for position in locations) + max(position.x for position in locations)) * 0.5
span = max(position.x for position in locations) - min(position.x for position in locations)
editor.set_level_viewport_camera_info(unreal.Vector(middle, max(position.y for position in locations) + max(4000, span), 2000), unreal.Rotator(pitch=-22, yaw=-90))
