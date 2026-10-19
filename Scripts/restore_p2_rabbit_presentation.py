"""Persist the P2 rabbit's character scale and landing contract in the map."""
import json
import pathlib
import unreal

MAP = "/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2"
MESH = "/Game/Passages/Echo/ArtP2/Rabbit/SK_P2_Rabbit"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(MAP), MAP
stage = next((a for a in actors.get_all_level_actors() if isinstance(a, unreal.CardRoomStage)), None)
assert stage
rabbit = unreal.load_asset(MESH)
assert rabbit
stage.set_editor_property("rabbit_skeletal_mesh", rabbit)
stage.set_editor_property("rabbit_visual_scale", 10.0)
stage.set_editor_property("rabbit_visual_offset", unreal.Vector(0, 0, 0))
stage.set_editor_property("rabbit_landing_local", unreal.Vector(1700, 0, -300))
stage.set_editor_property("rabbit_physics_drop", True)
stage.set_actor_label("P2 Countryside / Card opponent / Rabbit")
tags = list(stage.tags)
if unreal.Name("P2RabbitOpponent") not in tags:
    tags.append(unreal.Name("P2RabbitOpponent"))
stage.tags = tags
assert levels.save_current_level()
out = pathlib.Path(unreal.Paths.project_saved_dir()) / "Automation" / "CountrysideP2"
out.mkdir(parents=True, exist_ok=True)
report = {
    "map": MAP,
    "stage": stage.get_path_name(),
    "mesh": rabbit.get_path_name(),
    "rabbit_visual_scale": stage.get_editor_property("rabbit_visual_scale"),
    "rabbit_landing_local": str(stage.get_editor_property("rabbit_landing_local")),
    "physics_drop": stage.get_editor_property("rabbit_physics_drop"),
}
(out / "rabbit_presentation_restore.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("P2_RABBIT_PRESENTATION_RESTORED", json.dumps(report, ensure_ascii=False))
