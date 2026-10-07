import json
from pathlib import Path

import unreal


MAP_PATH = "/Game/MiningPrototype/Maps/Lvl_MiningDescent_AutoRooms"
MATERIALS = "/Game/MiningPrototype/Materials/EditableDescent/"
ROOMS = [
    ("ROOM_00_START_MOVE_ME", (0, 0, 270), (1000, 1100, 540)),
    ("ROOM_01_MOVE_AND_RESIZE_ME", (1700, 0, 50), (1000, 1100, 540)),
    ("ROOM_02_MOVE_AND_RESIZE_ME", (3500, 450, -220), (1100, 1200, 580)),
    ("ROOM_03_MOVE_AND_RESIZE_ME", (5700, 600, -500), (1400, 1400, 640)),
]


def create():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("Refusing to overwrite an existing map: " + MAP_PATH)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not levels.new_level(MAP_PATH):
        raise RuntimeError("Could not create the room-driven map")
    world = editor.get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", unreal.MiningPrototypeGameMode)
    world.get_world_settings().set_editor_property("kill_z", -100000)
    generator = actors.spawn_actor_from_class(unreal.MiningTunnelGenerator, unreal.Vector())
    generator.set_editor_property("auto_rebuild", False)
    generator.set_actor_label("AUTO_TUNNELS_Settings_Width_Height_Bedrock")
    generator.set_folder_path("AutoMine/Main")
    generator.set_editor_property("bedrock_material", unreal.load_asset(MATERIALS + "MI_Bedrock_Fixed"))
    generator.set_editor_property("tunnel_material", unreal.load_asset(MATERIALS + "MI_Diggable_Ochre"))
    controls = []
    for order, (label, position, size) in enumerate(ROOMS):
        room = actors.spawn_actor_from_class(unreal.MiningLayoutRoom, unreal.Vector(*position))
        room.set_actor_label(label)
        room.set_folder_path("AutoMine/Main/Rooms_EDIT_THESE")
        room.set_editor_property("connection_order", order)
        room.set_editor_property("room_size", unreal.Vector(*size))
        controls.append(room)
    if not generator.rebuild_tunnels():
        raise RuntimeError(generator.last_build_status)
    generator.set_editor_property("auto_rebuild", True)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(2000, 0, 3000), unreal.Rotator(pitch=-55, yaw=-35))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_intensity(2.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(2000, 0, 2000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_intensity(0.6)
    atmosphere = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    exposure = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
    exposure.set_editor_property("unbound", True)
    settings = exposure.get_editor_property("settings")
    for name in ("min_brightness", "max_brightness"):
        settings.set_editor_property("override_auto_exposure_" + name, True)
        settings.set_editor_property("auto_exposure_" + name, 3.0)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 0.0)
    exposure.set_editor_property("settings", settings)
    for actor in (sun, sky, atmosphere, exposure):
        actor.set_folder_path("AutoMine/Lighting")
    editor.set_level_viewport_camera_info(unreal.Vector(2750, 6100, 2350), unreal.Rotator(pitch=-23, yaw=-90))
    actors.set_selected_level_actors(controls)
    assert levels.save_current_level(), "Save failed"
    report = {
        "map": MAP_PATH,
        "rooms": [{"name": label, "position": position, "size": size, "order": order} for order, (label, position, size) in enumerate(ROOMS)],
        "status": generator.last_build_status,
        "build_ms": generator.last_build_milliseconds,
        "fill_triangles": generator.generated_fill.triangle_count,
        "actor_count": len(actors.get_all_level_actors()),
    }
    output = Path(unreal.Paths.project_saved_dir()) / "Validation" / "auto_rooms_layout.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("AUTO_ROOMS_CREATED " + json.dumps(report))


create()
