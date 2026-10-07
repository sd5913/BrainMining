import unreal


ROOT = "/Game/MiningPrototype/Materials"
PATH = ROOT + "/M_ChamberMist"
assets = unreal.AssetToolsHelpers.get_asset_tools()
editing = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level("/Game/MiningPrototype/Maps/Lvl_MiningLab")
material = unreal.load_asset(PATH) if unreal.EditorAssetLibrary.does_asset_exist(PATH) else assets.create_asset("M_ChamberMist", ROOT, unreal.Material, unreal.MaterialFactoryNew())
editing.delete_all_material_expressions(material)
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)
material.set_editor_property("used_with_instanced_static_meshes", True)


def expression(kind, horizontal, vertical):
    return editing.create_material_expression(material, kind, horizontal, vertical)


coordinates = expression(unreal.MaterialExpressionTextureCoordinate, -900, 0)
time = expression(unreal.MaterialExpressionTime, -900, 120)
opacity = expression(unreal.MaterialExpressionPerInstanceCustomData, -900, 240)
opacity.set_editor_property("data_index", 0)
interpolator = expression(unreal.MaterialExpressionVertexInterpolator, -680, 240)
assert editing.connect_material_expressions(opacity, "", interpolator, "VS")
cloud = expression(unreal.MaterialExpressionCustom, -420, 0)
cloud.set_editor_property("description", "Soft animated procedural mist")
cloud.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
inputs = []
for input_name in ("UV", "Clock", "Alpha"):
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", input_name)
    inputs.append(custom_input)
cloud.set_editor_property("inputs", inputs)
cloud.set_editor_property("code", "float2 shifted = UV - 0.5; float2 warped = UV + 0.025 * float2(sin(UV.y * 19 + Clock * 0.25), cos(UV.x * 17 - Clock * 0.2)); float edge = 1 - smoothstep(0.06, 0.49, length(shifted)); float wisps = 0.65 + 0.2 * sin(warped.x * 21 + sin(warped.y * 13) + Clock * 0.35) + 0.15 * cos(warped.y * 23 - warped.x * 9 - Clock * 0.3); return saturate(edge * edge * wisps * Alpha);")
assert editing.connect_material_expressions(coordinates, "", cloud, "UV")
assert editing.connect_material_expressions(time, "", cloud, "Clock")
assert editing.connect_material_expressions(interpolator, "", cloud, "Alpha")
fade = expression(unreal.MaterialExpressionDepthFade, -130, 0)
fade.set_editor_property("fade_distance_default", 20.0)
assert editing.connect_material_expressions(cloud, "", fade, "Opacity")
assert editing.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
tint = expression(unreal.MaterialExpressionConstant3Vector, -200, -180)
tint.set_editor_property("constant", unreal.LinearColor(0.9, 1.8, 1.5, 1.0))
assert editing.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
editing.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

rock = next(actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == "DiggableRock_Main")
rock.modify()
chamber = rock.get_component_by_class(unreal.FogChamberComponent)
chamber.modify()
chamber.set_editor_property("enabled", True)
chamber.set_editor_property("room_size", unreal.Vector(400, 480, 380))
chamber.set_editor_property("room_offset", unreal.Vector(0, 0, -90))
chamber.set_editor_property("fog_material", material)
rock.reset_rock()
assert chamber.room_ready, "Room must be enclosed by rock"

light = next((actor for actor in actors.get_all_level_actors() if actor.get_actor_label() == "Chamber - interior light"), None)
if light is None:
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(650, 0, 300))
    light.set_actor_label("Chamber - interior light")
light.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
light.light_component.set_editor_property("cast_shadows", True)
light.light_component.set_editor_property("attenuation_radius", 450.0)
light.light_component.set_intensity(130.0)
light.light_component.set_light_color(unreal.LinearColor(0.35, 0.75, 0.65))
assert levels.save_current_level()
unreal.log("FOG_CHAMBER_SETUP_COMPLETE")
