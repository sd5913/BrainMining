"""Import the supplied P2 rabbit as a skeletal mesh without touching existing art."""
from pathlib import Path
import json
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = ROOT / "Art" / "Passages" / "Echo" / "P2Rabbit" / "rabbit_anim.fbx"
DEST = "/Game/Passages/Echo/ArtP2/Rabbit"
ASSET = DEST + "/SK_P2_Rabbit"


def prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception:
        pass


assert SOURCE.exists(), SOURCE
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
task = unreal.AssetImportTask()
task.filename = str(SOURCE)
task.destination_path = DEST
task.destination_name = "SK_P2_Rabbit"
task.automated = True
task.save = True
task.replace_existing = True
options = unreal.FbxImportUI()
prop(options, "automated_import_should_detect_type", False)
prop(options, "import_mesh", True)
prop(options, "import_as_skeletal", True)
prop(options, "original_import_type", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
prop(options, "import_animations", False)
prop(options, "import_materials", True)
prop(options, "import_textures", False)
prop(options, "create_physics_asset", True)
prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
skeletal = options.skeletal_mesh_import_data
skeletal.set_editor_property("import_morph_targets", True)
skeletal.set_editor_property("update_skeleton_reference_pose", True)
skeletal.set_editor_property("preserve_smoothing_groups", True)
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(ASSET)
assert mesh, ASSET

unreal.EditorAssetLibrary.save_loaded_asset(mesh)
physics = None
try:
    physics = mesh.get_editor_property("physics_asset")
except Exception:
    pass
if not physics:
    try:
        smes = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        physics = smes.create_physics_asset(mesh, set_to_mesh=True)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        if physics:
            unreal.EditorAssetLibrary.save_loaded_asset(physics)
    except Exception as exc:
        unreal.log_warning("P2 rabbit physics asset creation failed: " + str(exc))
try:
    morph_names = [str(x) for x in mesh.get_all_morph_target_names()]
except Exception:
    morph_names = []
report = {
    "source": str(SOURCE),
    "skeletal_mesh": mesh.get_path_name(),
    "physics_asset": physics.get_path_name() if physics else "",
    "morph_targets": morph_names,
}
out = ROOT / "Saved" / "Automation" / "CountrysideP2"
out.mkdir(parents=True, exist_ok=True)
(out / "rabbit_import.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("P2_RABBIT_IMPORTED " + json.dumps(report, ensure_ascii=False))
