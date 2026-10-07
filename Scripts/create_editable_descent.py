import json
import math
import shutil
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_Editable"
MATERIAL_PATH = "/Game/MiningPrototype/Materials/EditableDescent"
LAYOUT_TAG = "EditableDescent"
SAVED = Path(unreal.Paths.project_saved_dir())
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assets = unreal.AssetToolsHelpers.get_asset_tools()
cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube")
rock_class = unreal.load_class(None, "/Script/BrainMining.DiggableRock")
manifest = {"map": MAP_PATH, "sections": [], "rocks": [], "cutaway": [], "units": "cm"}


def point(origin, rotation, local):
    pitch = math.radians(rotation.pitch)
    yaw = math.radians(rotation.yaw)
    forward = (math.cos(pitch) * math.cos(yaw), math.cos(pitch) * math.sin(yaw), math.sin(pitch))
    right = (-math.sin(yaw), math.cos(yaw), 0)
    up = (-math.sin(pitch) * math.cos(yaw), -math.sin(pitch) * math.sin(yaw), math.cos(pitch))
    return unreal.Vector(*(origin[axis] + forward[axis] * local[0] + right[axis] * local[1] + up[axis] * local[2] for axis in range(3)))


def vector_values(vector):
    return [vector.x, vector.y, vector.z]


def organize(actor, label, folder, parent=None, tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [LAYOUT_TAG, *tags])
    if parent:
        actor.attach_to_actor(parent, "None", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    return actor


def master_material():
    path = MATERIAL_PATH + "/M_EditableBlockout"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    surface = assets.create_asset("M_EditableBlockout", MATERIAL_PATH, unreal.Material, unreal.MaterialFactoryNew())
    tint = unreal.MaterialEditingLibrary.create_material_expression(surface, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name", "BaseColor")
    tint.set_editor_property("default_value", unreal.LinearColor(0.2, 0.2, 0.2, 1))
    roughness = unreal.MaterialEditingLibrary.create_material_expression(surface, unreal.MaterialExpressionScalarParameter)
    roughness.set_editor_property("parameter_name", "Roughness")
    roughness.set_editor_property("default_value", 0.85)
    glow = unreal.MaterialEditingLibrary.create_material_expression(surface, unreal.MaterialExpressionScalarParameter)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 0.0)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(surface, unreal.MaterialExpressionMultiply)
    unreal.MaterialEditingLibrary.connect_material_expressions(tint, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(glow, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(surface)
    unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


def material(name, color, glow=0):
    path = MATERIAL_PATH + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    surface = assets.create_asset(name, MATERIAL_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(surface, master)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(surface, "BaseColor", unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(surface, "Roughness", 0.86)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(surface, "Glow", glow)
    unreal.EditorAssetLibrary.save_loaded_asset(surface)
    return surface


def section_root(name, kind, origin, rotation, size):
    root = actors.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*origin), rotation)
    root.root_component.set_mobility(unreal.ComponentMobility.STATIC)
    organize(root, name + "__MOVE_WHOLE_SECTION", "Descent/" + name, tags=("SectionRoot", kind))
    manifest["sections"].append({"name": name, "kind": kind, "origin": list(origin), "pitch": rotation.pitch, "size": list(size), "root": root.get_path_name()})
    return root


def block(root, suffix, local, size, surface, cutaway=False, blocking=True):
    origin = vector_values(root.get_actor_location())
    rotation = root.get_actor_rotation()
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, point(origin, rotation, local), rotation)
    section_name = root.get_actor_label().split("__")[0]
    tags = ["Bedrock" if blocking else "Decoration"]
    if cutaway:
        tags.append("EditorCutaway")
    organize(actor, section_name + "__" + suffix, "Descent/" + section_name, root, tags)
    component = actor.static_mesh_component
    component.set_static_mesh(cube_mesh)
    component.set_material(0, surface)
    component.set_collision_profile_name("BlockAll" if blocking else "NoCollision")
    component.set_editor_property("generate_overlap_events", False)
    actor.set_actor_scale3d(unreal.Vector(*(extent / 100 for extent in size)))
    if cutaway:
        manifest["cutaway"].append(actor.get_path_name())
    return actor


def lamp(root, suffix, local, color, intensity=900):
    position = point(vector_values(root.get_actor_location()), root.get_actor_rotation(), local)
    actor = actors.spawn_actor_from_class(unreal.PointLight, position)
    section_name = root.get_actor_label().split("__")[0]
    organize(actor, section_name + "__" + suffix, "Descent/" + section_name, root)
    component = actor.light_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(intensity)
    component.set_editor_property("attenuation_radius", 1000.0)
    component.set_light_color(unreal.LinearColor(*color, 1))
    return actor


def sign(root, suffix, text, local, size=28):
    rotation = root.get_actor_rotation()
    position = point(vector_values(root.get_actor_location()), rotation, local)
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, position, unreal.Rotator(pitch=0, yaw=rotation.yaw + 180, roll=0))
    section_name = root.get_actor_label().split("__")[0]
    organize(actor, section_name + "__" + suffix, "Descent/" + section_name, root)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_text(text)
    component.set_world_size(size)
    component.set_text_render_color(unreal.Color(195, 232, 225, 255))
    component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def portal(root, position_x, width, height, accent):
    for side in (-1, 1):
        block(root, "Portal_" + str(position_x) + "_" + str(side), (position_x, side * (width / 2 - 7), height / 2), (10, 12, height), accent, blocking=False)
    block(root, "Portal_" + str(position_x) + "_Lintel", (position_x, 0, height - 8), (10, width, 12), accent, blocking=False)


def room(name, origin, length, width, height, accent, front_open=True, back_open=True):
    root = section_root(name, "Room", origin, unreal.Rotator(), (length, width, height))
    thickness = 80
    block(root, "Floor", (length / 2, 0, -40), (length + 40, width + thickness * 2, 80), floor_material)
    block(root, "Roof_EDITOR_HIDE", (length / 2, 0, height + 40), (length + 40, width + thickness * 2, 80), bedrock_material, True)
    block(root, "FarWall", (length / 2, -width / 2 - 40, height / 2), (length + 40, 80, height), wall_material)
    block(root, "NearWall_EDITOR_HIDE", (length / 2, width / 2 + 40, height / 2), (length + 40, 80, height), wall_material, True)
    for side, open_end in ((0, front_open), (length, back_open)):
        if open_end:
            side_width = (width - 620) / 2
            for edge in (-1, 1):
                block(root, "DoorWall_" + str(side) + "_" + str(edge), (side, edge * (310 + side_width / 2), height / 2), (80, side_width, height), bedrock_material)
            block(root, "DoorHeader_" + str(side), (side, 0, 250 + height / 2), (80, 620, height - 500), bedrock_material)
            portal(root, side + (45 if side == 0 else -45), 610, 490, accent)
        else:
            block(root, "EndWall_" + str(side), (side + (-40 if side == 0 else 40), 0, height / 2), (80, width + 160, height), bedrock_material)
    for side in (-1, 1):
        block(root, "FloorGuide_" + str(side), (length / 2, side * (width / 2 - 30), 2), (length - 60, 10, 3), accent, blocking=False)
        lamp(root, "Light_" + str(side), (length * (0.35 if side == -1 else 0.7), side * width * 0.27, height - 80), (0.75, 0.87, 1.0))
    return root, (origin[0] + length, origin[1], origin[2])


def tunnel(name, origin, length, slope, accent):
    width, height, thickness = 600, 440, 80
    rotation = unreal.Rotator(pitch=-slope, yaw=0, roll=0)
    root = section_root(name, "Tunnel", origin, rotation, (length, width, height))
    block(root, "Floor", (length / 2, 0, -40), (length + 40, width + thickness * 2, 80), floor_material)
    block(root, "Roof_EDITOR_HIDE", (length / 2, 0, height + 40), (length + 40, width + thickness * 2, 80), bedrock_material, True)
    block(root, "FarWall", (length / 2, -width / 2 - 40, height / 2), (length + 40, 80, height), bedrock_material)
    block(root, "NearWall_EDITOR_HIDE", (length / 2, width / 2 + 40, height / 2), (length + 40, 80, height), bedrock_material, True)
    fill = actors.spawn_actor_from_class(rock_class, point(origin, rotation, (length / 2, 0, height / 2)), rotation)
    organize(fill, name + "__DIGGABLE_EDIT_RockSize", "Descent/" + name, root, ("MineableFill",))
    fill.set_editor_property("rock_size", unreal.Vector(length - 120, width, height))
    fill.set_editor_property("rock_material", diggable_material)
    fill.get_component_by_class(unreal.FogChamberComponent).set_editor_property("enabled", False)
    fill.reset_rock()
    manifest["rocks"].append({"actor": fill.get_path_name(), "name": fill.get_actor_label(), "section": name, "entrance": list(origin), "length": length, "slope": slope})
    portal(root, 30, width - 10, height - 10, accent)
    sign(root, "EntryLabel", "DIG / " + name.split("_")[-1], (25, 0, height - 55), 25)
    lamp(root, "EntryLight", (20, 0, height - 55), (1.0, 0.8, 0.55), 650)
    return root, vector_values(point(origin, rotation, (length, 0, 0)))


def create_layout():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Editable map already exists; refusing to overwrite your edits: " + MAP_PATH)
    current_world = editor.get_editor_world()
    previous = {"map": current_world.get_path_name(), "actors": [{"label": actor.get_actor_label(), "class": actor.get_class().get_name(), "location": vector_values(actor.get_actor_location())} for actor in actors.get_all_level_actors()]}
    del current_world
    backup_dir = SAVED / "EditableDescentBackup" / "2026-10-04"
    backup_dir.mkdir(parents=True, exist_ok=True)
    snapshot = backup_dir / "original_scene.json"
    if not snapshot.exists():
        snapshot.write_text(json.dumps(previous, ensure_ascii=False, indent=2), encoding="utf-8")
    else:
        previous = json.loads(snapshot.read_text(encoding="utf-8"))
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("Could not save your existing changes; no new level created")
    source_map = Path(unreal.Paths.project_content_dir()) / "MiningPrototype" / "Maps" / "Lvl_MiningDescent.umap"
    if source_map.exists():
        shutil.copy2(source_map, backup_dir / "Lvl_MiningDescent_with_user_edits.umap")
    if not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create editable descent map")
    global master, bedrock_material, diggable_material, floor_material, wall_material
    unreal.EditorAssetLibrary.make_directory(MATERIAL_PATH)
    master = master_material()
    bedrock_material = material("MI_Bedrock_Fixed", (0.11, 0.16, 0.19))
    wall_material = material("MI_RoomWall", (0.19, 0.24, 0.26))
    diggable_material = material("MI_Diggable_Ochre", (0.38, 0.23, 0.105))
    floor_material = material("MI_Floor", (0.15, 0.18, 0.20))
    amber = material("MI_Accent_Amber", (0.95, 0.42, 0.09), 2.0)
    teal = material("MI_Accent_Teal", (0.08, 0.60, 0.46), 2.0)
    blue = material("MI_Accent_Blue", (0.13, 0.34, 0.82), 2.0)
    start, endpoint = room("00_START", (0, 0, 0), 700, 1000, 540, amber, False, True)
    sign(start, "Instructions", "START / DIG DOWN\nWASD + MOUSE / HOLD LMB\nR = RESET", (450, 0, 340), 26)
    player = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(180, 0, 105), unreal.Rotator())
    organize(player, "00_PlayerStart", "Descent/00_START", start)
    first_tunnel, endpoint = tunnel("01_TUNNEL_A", endpoint, 700, 12, amber)
    first_room, endpoint = room("02_ROOM_1", endpoint, 850, 1100, 540, teal)
    sign(first_room, "RoomLabel", "ROOM 1 / DISCOVERY", (760, 0, 420), 32)
    sign(first_room, "EditSpace", "RESERVED / ADD YOUR CONTENT", (760, 0, 365), 20)
    second_tunnel, endpoint = tunnel("03_TUNNEL_B", endpoint, 900, 18, teal)
    second_room, endpoint = room("04_ROOM_2", endpoint, 1000, 1200, 540, blue)
    sign(second_room, "RoomLabel", "ROOM 2 / INTERACTION", (910, 0, 420), 32)
    sign(second_room, "EditSpace", "RESERVED / ADD YOUR CONTENT", (910, 0, 365), 20)
    third_tunnel, endpoint = tunnel("05_TUNNEL_C", endpoint, 1000, 22, blue)
    final_room, endpoint = room("06_ROOM_3", endpoint, 1200, 1400, 600, amber, True, False)
    sign(final_room, "RoomLabel", "ROOM 3 / BREAKTHROUGH", (1110, 0, 430), 36)
    sign(final_room, "EditSpace", "RESERVED / FINAL ENCOUNTER", (1110, 0, 365), 22)
    manifest["finish"] = [endpoint[0] - 200, 0, endpoint[2] + 100]
    world = editor.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/BrainMining.MiningPrototypeGameMode"))
    world.get_world_settings().set_editor_property("kill_z", -5000)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(2000, 0, 3000), unreal.Rotator(pitch=-55, yaw=-35, roll=0))
    organize(sun, "Lighting_Sun", "Descent/90_Lighting")
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(2.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(2000, 0, 2000))
    organize(sky, "Lighting_Sky", "Descent/90_Lighting")
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_intensity(0.6)
    organize(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector()), "Lighting_Atmosphere", "Descent/90_Lighting")
    exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
    organize(exposure, "Lighting_Exposure", "Descent/90_Lighting")
    exposure.set_editor_property("unbound", True)
    settings = exposure.get_editor_property("settings")
    for property_name in ("min_brightness", "max_brightness"):
        settings.set_editor_property("override_auto_exposure_" + property_name, True)
        settings.set_editor_property("auto_exposure_" + property_name, 3.0)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 0.0)
    exposure.set_editor_property("settings", settings)
    editor.set_level_viewport_camera_info(unreal.Vector(3100, 4200, 2000), unreal.Rotator(pitch=-29, yaw=-90, roll=0))
    actors.set_selected_level_actors([first_tunnel])
    if not levels.save_current_level():
        raise RuntimeError("Could not save new editable map")
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag("EditorCutaway"):
            actor.set_is_temporarily_hidden_in_editor(True)
    manifest["actor_count"] = len([actor for actor in actors.get_all_level_actors() if actor.actor_has_tag(LAYOUT_TAG)])
    manifest["original_map"] = previous["map"]
    output = SAVED / "Validation" / "editable_descent_layout.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    unreal.log("EDITABLE_DESCENT_CREATED " + str(output))


create_layout()
