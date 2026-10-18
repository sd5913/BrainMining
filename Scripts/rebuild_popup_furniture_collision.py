"""Rebuild stale imported furniture hulls from the current render geometry.

Run in the editor with PIE stopped. MainGame references the same source-room
assets, so no copied room or map transforms need to change.
"""
import json
from pathlib import Path
import unreal

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert editor.get_game_world() is None, 'Stop PIE before rebuilding asset collision'
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
report = []
for name in ('SM_OfficeDesk', 'SM_DeskChair', 'SM_Copier'):
    mesh = unreal.load_asset('/Game/PopupClose/OfficeArt/Meshes/' + name)
    assert mesh, name
    old_count = meshes.get_convex_collision_count(mesh)
    # FBX reimport can retain collision from an older, larger version of a mesh.
    # Explicitly discard it before fitting new hulls to the current vertices.
    assert meshes.remove_collisions(mesh), name
    assert meshes.set_convex_decomposition_collisions(mesh, 8, 16, 200000), name
    new_count = meshes.get_convex_collision_count(mesh)
    assert new_count > 0, name
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), name
    report.append(dict(mesh=mesh.get_path_name(), previous_hulls=old_count,
                       rebuilt_hulls=new_count,
                       render_size_cm=list((mesh.get_bounds().box_extent * 2).to_tuple())))
out = Path(unreal.Paths.project_dir()) / 'Saved/Validation/PopupCollisionAudit'
out.mkdir(parents=True, exist_ok=True)
(out / 'rebuilt_collision.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('POPUP_FURNITURE_COLLISION_REBUILT ' + json.dumps(report))
