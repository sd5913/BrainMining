"""Read-only verification for the HeavenToilet pipe material and inlet drip."""
import json
import pathlib
import unreal

MAP = "/Game/CollectionPrototype/HeavenToilet/Maps/Lvl_HeavenToilet"
MAT = "/Game/CollectionPrototype/HeavenToilet/Materials/M_HeavenPipe"
DRIP = "/Game/HeavenPreview/VFX/NS_HeavenPipeDrip"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(MAP), MAP
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
pipe_mat = unreal.load_asset(MAT)
drip_asset = unreal.load_asset(DRIP)
assert pipe_mat and drip_asset

pipe_components = []
bad = []
for actor in actors:
    label = actor.get_actor_label()
    folder = str(actor.get_folder_path())
    if not (folder.endswith("PipesDetailed") or label.startswith("写实管道") or label.startswith("写实弯管")):
        continue
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if not comp.static_mesh:
            continue
        material = comp.get_material(0)
        row = {"actor": label, "mesh": comp.static_mesh.get_path_name(), "material": material.get_path_name() if material else ""}
        pipe_components.append(row)
        if material != pipe_mat:
            bad.append(row)

drips = []
for actor in actors:
    if actor.get_actor_label() != "入口露出管口 · 持续滴水":
        continue
    for comp in actor.get_components_by_class(unreal.NiagaraComponent):
        asset = comp.get_editor_property("asset")
        drips.append({"actor": actor.get_path_name(), "location": str(actor.get_actor_location()), "asset": asset.get_path_name() if asset else "", "auto_activate": comp.get_editor_property("auto_activate"), "visible": comp.is_visible()})

# The detailed pipe run is 30 placed mesh components; two additional hidden
# migration submeshes are restored by the authoring pass and are not counted
# here because they are intentionally hidden source geometry.
report = {"map": MAP, "pipe_components": len(pipe_components), "bad_materials": bad, "drips": drips, "passed": len(pipe_components) == 30 and not bad and any(d["asset"] == drip_asset.get_path_name() and d["auto_activate"] and d["visible"] for d in drips)}
out = pathlib.Path(unreal.Paths.project_saved_dir()) / "HeavenPreview"
(out / "pipe_material_drip_verify.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("HEAVEN_PIPE_MATERIAL_DRIP_VERIFY", json.dumps(report, ensure_ascii=False))
assert report["passed"]
