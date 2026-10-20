import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/StartRoom/Maps/Lvl_StartRoom')
p=next(a for a in A.get_all_level_actors() if a.actor_has_tag('StartRoomView_Pear'))
p.set_actor_rotation(unreal.Rotator(yaw=-45),False)
# Rest the actual mesh on the cabinet shelf after rotation and scale.
center,extent=p.get_actor_bounds(False);location=p.get_actor_location();location.z+=75.8572-(center.z-extent.z);p.set_actor_location(location,False,True)
light=next((a for a in A.get_all_level_actors() if a.get_actor_label()=='STARTROOM_CabinetSoftFill'),None) or A.spawn_actor_from_class(unreal.PointLight,location+unreal.Vector(-10,-20,25))
light.set_actor_label('STARTROOM_CabinetSoftFill');c=light.point_light_component;c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_intensity(.08);c.set_attenuation_radius(60);c.set_cast_shadows(False);c.set_light_color(unreal.LinearColor(1,.85,.63))
assert L.save_current_level()
unreal.SystemLibrary.quit_editor()
