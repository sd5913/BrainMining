import unreal
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels={'Study V07 / Cat warm key':480000,'Study V07 / Cat amber rim':360000,'P2 Countryside / Lighting/Tabletop soft key':300000,'P2 Countryside / Lighting/Soft window daylight':3800000,'P2 Countryside / Lighting/Golden window bounce':3200000}
for a in A.get_all_level_actors():
 if a.get_actor_label() in levels:a.light_component.set_intensity(levels[a.get_actor_label()])
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log('P2_FINAL_LIGHT_HIGHLIGHTS_BALANCED')
