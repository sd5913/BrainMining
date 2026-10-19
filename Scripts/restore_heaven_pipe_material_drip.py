"""Restore HeavenToilet pipe material slots and add a persistent inlet drip.

This script edits only the authored HeavenToilet map and the three dedicated
pipe meshes. It creates a timestamped map backup before saving.
"""
import datetime
import json
import pathlib
import shutil
import unreal

MAP = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
PIPE_MATERIAL = "/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenPipe"
DRIP_ASSET = "/Game/HeavenPreview/VFX/NS_HeavenPipeDrip"
PIPE_MESHES = [
    "/Game/HeavenPreview/Meshes/Pipes/SM_HeavenPipePart_002",
    "/Game/HeavenPreview/Meshes/Pipes/SM_HeavenPipePart_009",
    "/Game/HeavenPreview/Meshes/Pipes/SM_HeavenPipePart_013",
]

root = pathlib.Path(unreal.Paths.project_dir()).resolve()
map_file = root / "Content" / "CollectionPrototype" / "HeavenToilet" / "Maps" / "Lvl_HeavenToilet.umap"
backup = root / "Saved" / "HeavenPreview" / ("BeforePipeMaterialRestore_" + datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
backup.mkdir(parents=True, exist_ok=True)
shutil.copy2(map_file, backup / map_file.name)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP), MAP
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_editor_world().get_path_name().startswith(MAP + ".")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
pipe_material = unreal.load_asset(PIPE_MATERIAL)
drip_asset = unreal.load_asset(DRIP_ASSET)
assert pipe_material, PIPE_MATERIAL
assert drip_asset, DRIP_ASSET

# Restore the imported mesh defaults as well as actor overrides. The imported
# meshes had empty material slots after the level migration, which made every
# detailed pipe render with the fallback checker/gray material.
mesh_count = 0
for path in PIPE_MESHES:
    mesh = unreal.load_asset(path)
    if not mesh:
        continue
    mesh.set_material(0, pipe_material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    mesh_count += 1

restored = []
for actor in actors.get_all_level_actors():
    label = actor.get_actor_label()
    folder = str(actor.get_folder_path())
    is_detailed = folder.endswith("PipesDetailed") or label.startswith("写实管道") or label.startswith("写实弯管")
    is_migrated_pipe = label.startswith("modular_pipes_junct_oly_and_hard_surface_Submesh")
    if not (is_detailed or is_migrated_pipe):
        continue
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if component.static_mesh:
            component.set_material(0, pipe_material)
            restored.append(label)

# The inlet is the exposed vertical leg at (-300, 125); leave the user's pipe
# transforms intact and keep this effect as a separate Niagara actor.
drip_actor = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == "入口露出管口 · 持续滴水"), None)
if not drip_actor:
    drip_actor = actors.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(-300, 125, 126))
    drip_actor.set_actor_label("入口露出管口 · 持续滴水")
    drip_actor.set_folder_path("Heaven/Water")
drip_component = drip_actor.get_component_by_class(unreal.NiagaraComponent)
assert drip_component
drip_component.set_asset(drip_asset)
drip_component.set_auto_activate(True)
drip_component.set_visibility(True)
drip_component.activate(True)

unreal.EditorAssetLibrary.save_loaded_asset(drip_asset, False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()

report = {
    "map": MAP,
    "pipe_material": pipe_material.get_path_name(),
    "restored_actor_components": len(restored),
    "restored_mesh_defaults": mesh_count,
    "drip_actor": drip_actor.get_path_name(),
    "drip_location": str(drip_actor.get_actor_location()),
    "drip_asset": drip_asset.get_path_name(),
    "backup": str(backup),
}
out = root / "Saved" / "HeavenPreview"
(out / "pipe_material_drip_restore.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("HEAVEN_PIPE_MATERIAL_DRIP_RESTORED", json.dumps(report, ensure_ascii=False))
