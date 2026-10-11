"""Create a separate demonstration map. Existing maps are never overwritten."""
import json
from pathlib import Path
import unreal

MAP = "/Game/ExternalMining/Maps/Lvl_ExternalMining_Demo"
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    raise RuntimeError("演示关卡已存在，拒绝覆盖：" + MAP)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.new_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
region = actors.spawn_actor_from_class(unreal.MiningExternalRegion, unreal.Vector(0, 0, 1500))
region.set_actor_label("EDIT_外部通路矿区")
region.set_editor_property("rock_size", unreal.Vector(6000, 4000, 3000))
region.set_editor_property("shell_thickness", 200.0)
region.set_editor_property("chunk_length", 1600.0)
region.set_editor_property("show_cutter_preview", False)

# These primitive assets exercise the same source-mesh input used by imported FBX assets.
# They are deliberately labelled as demonstration shapes, not Blender-authored art.
main = unreal.MiningExternalCutter()
main.set_editor_property("mesh", unreal.load_asset("/Engine/BasicShapes/Cube"))
main.set_editor_property("local_transform", unreal.Transform(
    location=unreal.Vector(0, 0, -700), scale=unreal.Vector(72, 8, 9)))
chamber = unreal.MiningExternalCutter()
chamber.set_editor_property("mesh", unreal.load_asset("/Engine/BasicShapes/Sphere"))
chamber.set_editor_property("local_transform", unreal.Transform(
    location=unreal.Vector(0, 0, -350), scale=unreal.Vector(14, 14, 14)))
region.set_editor_property("cutters", [main, chamber])
rock_material = unreal.load_asset("/Game/MiningPrototype/Materials/DirectRooms/MI_Diggable_Warm")
if rock_material:
    region.set_editor_property("rock_material", rock_material)
region.set_editor_property("shell_material", unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial"))
assert region.generate_region(), region.build_status

start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-2700, 0, 500))
start.set_actor_label("PLAYER_START_洞内朝向出口")
start.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
for x in (-2400, -1200, 0, 1200, 2400):
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, 0, 1050))
    light.set_actor_label("DEMO_通路灯_" + str(x))
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_editor_property("intensity", 4000.0)
    component.set_editor_property("attenuation_radius", 1800.0)
    component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    component.set_editor_property("cast_shadows", False)
world.get_world_settings().set_editor_property("default_game_mode", unreal.MiningPrototypeGameMode)
assert levels.save_current_level()
report = {"passed": True, "map": MAP, "status": str(region.build_status),
          "chunks": len(region.generated_rock.chunks),
          "largest_chunk_triangles": region.generated_rock.largest_chunk_triangles,
          "cutters": "UE primitive source meshes; replace with imported Blender meshes"}
output = Path(unreal.Paths.project_saved_dir()) / "Validation"
output.mkdir(parents=True, exist_ok=True)
(output / "external_mining_demo.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("EXTERNAL_MINING_DEMO " + json.dumps(report, ensure_ascii=False))
