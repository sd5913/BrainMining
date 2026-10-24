"""Create a ceiling-only lighting fallback; leave the imported source material untouched."""
import unreal,json
from pathlib import Path
EA=unreal.EditorAssetLibrary;ML=unreal.MaterialEditingLibrary
path='/Game/PopupClose/OfficeArt/Architecture/MI_OfficeCeilingNoGI'
m=unreal.load_asset(path) if EA.does_asset_exist(path) else EA.duplicate_asset('/Game/Office/Materials/Mi_Ceiling',path)
assert m
# Unsupported Lumen leaves upward-facing bounce light absent in SM5/DX11.
# Retain the original tile diffuse/normal maps, with a modest indirect-light floor.
ML.set_material_instance_static_switch_parameter_value(m,'UseEmis',True)
assert ML.get_material_instance_static_switch_parameter_value(m,'UseEmis')
ML.set_material_instance_texture_parameter_value(m,'EmissiveTex',unreal.load_asset('/Game/Office/Textures/TX_Ceiling_DIF'))
for name,value in [('Emissive Intensity',2.5),('RoughnessLow',.85),('RoughnessHigh',1.),('MetalnessLow',0.),('MetalnessHigh',0.)]:
 ML.set_material_instance_scalar_parameter_value(m,name,value)
ML.update_material_instance(m)
assert EA.save_loaded_asset(m,False)
(Path(unreal.Paths.project_saved_dir())/'Validation/Collection24/ceiling-fallback.json').write_text(json.dumps(dict(asset=path,emission=2.5,original_unchanged=True),indent=2))
unreal.SystemLibrary.quit_editor()
