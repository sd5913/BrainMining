"""Apply the V04 hall layout to the existing passage map; do not regenerate gameplay actors.

Absolute transforms are stored in giant_hall_v04_layout.json. Reapplication is
idempotent, and preflight rejects actors edited since V03/V04 rather than silently
overwriting a teammate's layout. Run in the UE editor Python environment.
"""
import json
import shutil
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
plan = json.loads((root / "Scripts/giant_hall_v04_layout.json").read_text(encoding="utf-8-sig"))
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level(plan["map"])
by_name = {a.get_name(): a for a in actors.get_all_level_actors()}

def near(v, d):
    return all(abs(getattr(v, axis) - d[axis]) < .05 for axis in ("x", "y", "z"))

def matches(actor, t):
    r = actor.get_actor_rotation()
    return (near(actor.get_actor_location(), t["location"])
            and near(actor.get_actor_scale3d(), t["scale"])
            and all(abs(getattr(r, axis) - t["rotation"][axis]) < .05
                    for axis in ("pitch", "yaw", "roll")))

for row in plan["changes"]:
    actor = by_name.get(row["name"])
    if (not actor or actor.get_actor_label() != row["label"]
            or not any(matches(actor, row[k]) for k in ("before", "after"))):
        raise RuntimeError("Layout differs from V03/V04; inspect before applying: " + row["name"])

backup = root / "Saved/GiantHallV04Backup/Lvl_EchoMazeToCards_P1.umap"
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(root / "Content/Passages/Echo/Maps/Lvl_EchoMazeToCards_P1.umap", backup)

def vector(d):
    return unreal.Vector(d["x"], d["y"], d["z"])

for row in plan["changes"]:
    actor, t = by_name[row["name"]], row["after"]
    actor.set_actor_location(vector(t["location"]), False, False)
    actor.set_actor_scale3d(vector(t["scale"]))
    actor.set_actor_rotation(unreal.Rotator(**t["rotation"]), False)

entrance = next(a for a in by_name.values() if isinstance(a, unreal.MiningGiantRoomEntrance))
bounds = entrance.get_component_by_class(unreal.BoxComponent)
bounds.set_relative_location(vector(plan["roomBounds"]["relativeLocation"]), False, False)
bounds.set_box_extent(vector(plan["roomBounds"]["boxExtent"]), False)
assert levels.save_current_level()
unreal.log("GIANT_HALL_V04_SAVED: widened shell and separated furniture")

