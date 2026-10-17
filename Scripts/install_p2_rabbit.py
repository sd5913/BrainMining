"""Assign the imported rabbit to the P2 CardRoomStage and save the map."""
from pathlib import Path
import json
import runpy
import unreal

MAP = "/Game/Passages/Echo/Maps/Lvl_EchoMazeToCards_P2"
MESH = "/Game/Passages/Echo/ArtP2/Rabbit/SK_P2_Rabbit"
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist(MESH):
    runpy.run_path(str(Path(unreal.Paths.project_dir()) / "Scripts" / "import_p2_rabbit.py"))
assert LEVELS.load_level(MAP), MAP
stage = next((a for a in ACTORS.get_all_level_actors() if isinstance(a, unreal.CardRoomStage)), None)
assert stage, "P2 CardRoomStage not found"
rabbit = unreal.load_asset(MESH)
assert rabbit, MESH
stage.set_editor_property("rabbit_skeletal_mesh", rabbit)
stage.set_editor_property("rabbit_visual_scale", 1.5)
stage.set_editor_property("opponent_scale", 1.0)
stage.set_editor_property("rabbit_visual_offset", unreal.Vector(0, 0, 0))
# Physics drop is enabled by the native default; older UE Python bindings expose
# the bool under different names, so leave the default untouched here.
stage.set_actor_label("P2 Countryside / Card opponent / Rabbit")
stage.tags = list(set(stage.tags + [unreal.Name("P2RabbitOpponent")]))
assert LEVELS.save_current_level(), "P2 map save failed"
root = Path(unreal.Paths.project_dir()).resolve()
out = root / "Saved" / "Automation" / "CountrysideP2"
out.mkdir(parents=True, exist_ok=True)
(out / "rabbit_stage_install.json").write_text(json.dumps({
    "map": MAP,
    "stage": stage.get_path_name(),
    "mesh": rabbit.get_path_name(),
    "opponent_scale": 1.0,
    "scale": 1.5,
    "physics_drop": True,
}, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("P2_RABBIT_STAGE_INSTALLED")
