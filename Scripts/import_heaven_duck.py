"""Import the user supplied dark.fbx duck as a non-destructive HeavenPreview mesh."""
import json
import pathlib
import unreal


def import_duck():
    root = pathlib.Path(unreal.Paths.project_dir()).resolve()
    source = root / "Art" / "HeavenPreview" / "Props" / "SM_DarkDuck.fbx"
    assert source.exists(), source
    destination = "/Game/HeavenPreview/Meshes"
    path = destination + "/SM_DarkDuck"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mesh = unreal.load_asset(path)
        return mesh

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = "SM_DarkDuck"
    task.automated = True
    task.save = True
    task.replace_existing = False
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_materials = True
    options.import_textures = True
    options.import_animations = False
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("import_mesh_lods", False)
    task.options = options
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    assert mesh, path
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    report = {"source": str(source), "mesh": mesh.get_path_name()}
    (root / "Saved" / "HeavenPreview").mkdir(parents=True, exist_ok=True)
    (root / "Saved" / "HeavenPreview" / "duck_import.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print("HEAVEN_DUCK_IMPORTED", json.dumps(report, ensure_ascii=False))
    return mesh


import_duck()
