"""Use UE's supported decal pass for procedural mining-surface stains."""
import unreal
from pathlib import Path
material=unreal.load_asset('/Game/CollectionPrototype/Materials/M_CollectionStainOverlay')
assert material
material.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
# Keep this one editor session for the visual audit after the asset repair.
path=Path(unreal.Paths.project_dir())/'Scripts/audit_echo_focus.py'
exec(compile(path.read_text(encoding='utf-8'),str(path),'exec'))
