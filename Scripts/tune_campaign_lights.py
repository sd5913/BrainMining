"""One-time presentation correction for the generated campaign, matching its recipe."""
import unreal
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Campaign/Maps/Lvl_FullJourney')
count = 0
for actor in actors.get_all_level_actors():
    if not isinstance(actor,unreal.PointLight) or actor.get_actor_label() != 'Campaign_PathLight':
        continue
    light = actor.point_light_component
    if light.get_editor_property('use_inverse_squared_falloff'):
        intensity = light.get_editor_property('intensity')
        light.set_editor_property('use_inverse_squared_falloff',False)
        light.set_editor_property('intensity_units',unreal.LightUnits.UNITLESS)
        light.set_editor_property('light_falloff_exponent',2.0)
        light.set_editor_property('intensity',intensity/3500.0*.8)
        count += 1
assert levels.save_current_level()
unreal.log('CAMPAIGN_LIGHTS_TUNED ' + str(count))
unreal.SystemLibrary.quit_editor()
