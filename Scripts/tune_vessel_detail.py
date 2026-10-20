import unreal,json
from pathlib import Path
ML=unreal.MaterialEditingLibrary;EA=unreal.EditorAssetLibrary
m=unreal.load_asset('/Game/EndingEmergence/Materials/M_EE_SoftVein');assert m
assert not any(str(n.get_editor_property('desc'))=='Vessel low-contrast tissue' for n in ML.get_material_expressions(m)),'Already tuned'
for n in ML.get_material_expressions(m):
 if isinstance(n,unreal.MaterialExpressionTextureSample):n.set_editor_property('const_mip_value',3)
tone=ML.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
tone.set_editor_property('constant',unreal.LinearColor(.12,.012,.018));tone.set_editor_property('desc','Vessel low-contrast tissue')
for prop,strength in [(unreal.MaterialProperty.MP_BASE_COLOR,.5),(unreal.MaterialProperty.MP_SUBSURFACE_COLOR,.45)]:
 original=ML.get_material_property_input_node(m,prop)
 if not original:continue
 output=ML.get_material_property_input_node_output_name(m,prop)
 blend=ML.create_material_expression(m,unreal.MaterialExpressionLinearInterpolate);blend.set_editor_property('const_alpha',strength)
 assert ML.connect_material_expressions(original,output,blend,'A');assert ML.connect_material_expressions(tone,'',blend,'B')
 assert ML.connect_material_property(blend,'',prop)
ML.recompile_material(m);assert EA.save_loaded_asset(m,False)
out=Path(unreal.Paths.project_dir())/'Saved/Validation/VesselDetail/material.json';r=json.loads(out.read_text())
r.update(mip_bias=3,color_detail_strength=.5,subsurface_detail_strength=.55);out.write_text(json.dumps(r,indent=2),encoding='utf8')
unreal.SystemLibrary.quit_editor()
