import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningLab"
ASSET_ROOT = "/Game/MiningPrototype/Materials"
assets = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def material(name, color, emissive=False):
    path = ASSET_ROOT + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    result = assets.create_asset(name, ASSET_ROOT, unreal.Material, unreal.MaterialFactoryNew())
    tint = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant3Vector)
    tint.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if emissive:
        unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.88)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(result)
    unreal.EditorAssetLibrary.save_loaded_asset(result)
    return result


def cube(label, location, size, surface):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(label)
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    component.set_material(0, surface)
    actor.set_actor_scale3d(unreal.Vector(*(value / 100.0 for value in size)))
    return actor


def sign(label, text, location, size=34.0):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*location), unreal.Rotator(pitch=0, yaw=180, roll=0))
    actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(text)
    component.set_world_size(size)
    component.set_text_render_color(unreal.Color(120, 240, 225, 255))
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return actor


if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError("Mining lab already exists; refusing to overwrite it.")

unreal.EditorAssetLibrary.make_directory("/Game/MiningPrototype/Maps")
unreal.EditorAssetLibrary.make_directory(ASSET_ROOT)
if not levels.new_level(MAP_PATH):
    raise RuntimeError("Could not create mining lab")

rock_material = material("M_Rock", (0.26, 0.20, 0.14))
floor_material = material("M_Floor", (0.065, 0.085, 0.10))
wall_material = material("M_Wall", (0.11, 0.15, 0.18))
accent_material = material("M_Accent", (0.08, 0.70, 0.58), True)
warning_material = material("M_Warning", (0.85, 0.40, 0.05))

cube("Safety floor", (450, 0, -30), (3000, 2200, 60), floor_material)
cube("Left boundary", (450, -1125, 220), (3000, 50, 500), wall_material)
cube("Right boundary", (450, 1125, 220), (3000, 50, 500), wall_material)
cube("Entry boundary", (-1075, 0, 220), (50, 2300, 500), wall_material)
cube("Exit boundary", (1975, 0, 220), (50, 2300, 500), wall_material)
for lane_y in (-600, 600):
    cube("Route guide", (450, lane_y, 1), (2800, 5, 2), accent_material)
for marker_x in (-500, -250, 0, 1250, 1500, 1750):
    cube("Distance marker", (marker_x, 0, 1), (4, 1600, 2), wall_material)

rock_class = unreal.load_class(None, "/Script/BrainMining.DiggableRock")
rock = actors.spawn_actor_from_class(rock_class, unreal.Vector(650, 0, 300))
rock.set_actor_label("DiggableRock_Main")
rock.set_editor_property("rock_material", rock_material)
rock.reset_rock()

player_start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-200, 0, 100), unreal.Rotator(0, 0, 0))
player_start.set_actor_label("Mining spawn")
cube("Goal left post", (1450, -180, 155), (35, 35, 310), accent_material)
cube("Goal right post", (1450, 180, 155), (35, 35, 310), accent_material)
cube("Goal lintel", (1450, 0, 300), (35, 395, 30), accent_material)
cube("Entry threshold", (50, 0, 2), (10, 1000, 4), warning_material)
sign("Rock title", "01 / DIG THROUGH", (140, 0, 480), 42)
sign("Rock hint", "HOLD LMB TO EXCAVATE", (140, 0, 420), 24)
sign("Goal title", "BREAKTHROUGH", (1420, 0, 370), 36)

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-48, yaw=-35, roll=0))
sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(4.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 900))
sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_intensity(0.7)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
for light_x in (-450, 1450):
    lamp = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(light_x, 0, 500))
    lamp.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    lamp.light_component.set_intensity(300.0)
    lamp.light_component.set_editor_property("attenuation_radius", 1700.0)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
exposure.set_actor_label("Mining exposure")
exposure.set_editor_property("unbound", True)
settings = exposure.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM)
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", 3.0)
settings.set_editor_property("auto_exposure_max_brightness", 3.0)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", 0.0)
exposure.set_editor_property("settings", settings)
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/BrainMining.MiningPrototypeGameMode"))
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(-800, -1200, 1100), unreal.Rotator(pitch=-28, yaw=40, roll=0))
if not levels.save_current_level():
    raise RuntimeError("Could not save mining lab")
unreal.log("MINING_LAB_CREATED " + MAP_PATH)
